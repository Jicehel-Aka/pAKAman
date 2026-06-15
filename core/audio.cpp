
#include "core/audio.h"
#include "lib/common.h"
#include "lib/expander.h"
#include "lib/sdcard.h"
#include "lib/TAS2505_rehs.h"
#include "lib/audio_player.h"
#include "lib/audio_sfx.h"
#include "lib/audio_pmf.h"
#include "lib/audio_track_sfx.h"
#include "lib/audio_sfx_cache.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Gestion du son

AudioPMF audioPMF;
AudioSettings g_audio_settings;
static float g_music_duck = 1.0f;      // 1.0 = volume normal
static float g_music_duck_target = 1.0f;
static float g_music_duck_speed = 0.02f; // vitesse du fade


// -----------------------------------------------------------------------------
// Configuration interne / debug
// -----------------------------------------------------------------------------

// 0 = fonctionnement normal (FIFO), 1 = test audio cos44100 au démarrage
#define AUDIO_TEST_MODE 0

static const char* AUDIO_TAG = "audio";

// -----------------------------------------------------------------------------
// Petites fonctions utilitaires locales
// -----------------------------------------------------------------------------

static void delay_ms(uint32_t milliseconds)
{
    vTaskDelay(milliseconds / portTICK_PERIOD_MS);
}

// -----------------------------------------------------------------------------
// Tache séparée pour jouer la musique de façon régulière
// -----------------------------------------------------------------------------

static void audio_task(void* arg)
{
    const int period_ms =
        (1000 * GB_AUDIO_BUFFER_SAMPLE_COUNT) / GB_AUDIO_SAMPLE_RATE;

    TickType_t last = xTaskGetTickCount();

    while (true)
    {
        audio_update();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(period_ms));
    }
}


// -----------------------------------------------------------------------------
// Données globales audio : FIFO, I2S, test cos44100
// -----------------------------------------------------------------------------

// Canal I2S (TX uniquement)
static i2s_chan_handle_t g_i2s_tx_channel = nullptr;

// Table sinus 44100 Hz (une période sur 256 échantillons)
static int16_t g_cos44100_table[] = {
#include "lib/cos44100.h"
};

// NOTE : l’ancien mécanisme read_audio_file() / wav_pool_update() n’est plus
// utilisé pour le son du jeu. La lecture WAV est maintenant gérée par
// audio_track_wav dans le mixeur.
//
// On garde la déclaration si un jour tu veux le réutiliser, mais ce n’est
// plus le cœur du pipeline.
void read_audio_file(uint8_t* destination, uint32_t length_in_bytes);

// FIFO PCM 16 bits mono
// Taille totale = GB_AUDIO_BUFFER_SAMPLE_COUNT * GB_AUDIO_BUFFER_FIFO_COUNT
static int16_t g_audio_fifo[GB_AUDIO_BUFFER_SAMPLE_COUNT * GB_AUDIO_BUFFER_FIFO_COUNT];

// Index de lecture/écriture dans la FIFO (en échantillons 16 bits)
static uint32_t g_fifo_read_index  = 0;
static uint32_t g_fifo_write_index = 0;

// Statistiques FIFO
static uint16_t g_buffer_miss_count     = 0; // nombre de fois où la FIFO était vide
static uint16_t g_buffer_overflow_count = 0; // nombre de fois où la FIFO était pleine

// Etat du test audio (cos44100)
static bool     g_audio_test_enabled = false;
static uint16_t g_cos_read_index     = 0; // phase du sinus (16.8 fixed point)
static float    g_cos_pitch_factor   = 256.0f / 4.0f;
static uint16_t g_cos_pitch_step     = 0; // pas de phase

// Compteur I2S pour debug (nombre d’appels du callback)
static int g_i2s_callback_count = 0;

// -----------------------------------------------------------------------------
// Mixeur : audio_player + pistes internes
// -----------------------------------------------------------------------------

static audio_player    	s_audio_player;
audio_track_tone       	g_track_tone;
audio_track_noise      	g_track_noise;
audio_track_wav        	g_track_wav;
audio_track_sfx 		g_track_sfx;

// -----------------------------------------------------------------------------
// Helpers FIFO
// -----------------------------------------------------------------------------

static inline uint32_t fifo_size_in_samples()
{
    return (uint32_t)(sizeof(g_audio_fifo) / sizeof(g_audio_fifo[0]));
}

static inline uint32_t fifo_used_samples()
{
    return g_fifo_write_index - g_fifo_read_index;
}

static inline uint32_t fifo_free_samples()
{
    return fifo_size_in_samples() - fifo_used_samples();
}

// -----------------------------------------------------------------------------
// Gestion de la FIFO : alimentation
// -----------------------------------------------------------------------------

// Phase 3/4/5 : pour le son du jeu, on n’utilise plus wav_pool_update().
// On garde la fonction pour compatibilité, mais elle ne fait plus rien.
void wav_pool_update()
{
    // Anciennement : remplissage de la FIFO à partir d’un WAV “global”.
    // Maintenant : chaque piste (notamment audio_track_wav) lit ses données
    // directement depuis FILE* et le mixeur produit un buffer mixé unique.
}

// Pousse un buffer mixé dans la FIFO
void audio_push_buffer(const int16_t* audio_buffer)
{
    if (fifo_free_samples() >= GB_AUDIO_BUFFER_SAMPLE_COUNT)
    {
        uint32_t write_offset = g_fifo_write_index % fifo_size_in_samples();
        memcpy(&g_audio_fifo[write_offset],
               audio_buffer,
               2 * GB_AUDIO_BUFFER_SAMPLE_COUNT);
        g_fifo_write_index += GB_AUDIO_BUFFER_SAMPLE_COUNT;
    }
    else
    {
        // FIFO pleine : on perd ce buffer
        g_buffer_overflow_count++;
    }
}

// -----------------------------------------------------------------------------
// Statistiques FIFO (en buffers, pas en échantillons)
// -----------------------------------------------------------------------------

uint32_t audio_fifo_buffer_count(void)
{
    return fifo_size_in_samples() / GB_AUDIO_BUFFER_SAMPLE_COUNT;
}

uint32_t audio_fifo_buffer_used(void)
{
    return fifo_used_samples() / GB_AUDIO_BUFFER_SAMPLE_COUNT;
}

uint32_t audio_fifo_buffer_free(void)
{
    return audio_fifo_buffer_count() - audio_fifo_buffer_used();
}

// -----------------------------------------------------------------------------
// Mode test audio : activation/désactivation de la génération cos44100
// -----------------------------------------------------------------------------

void audio_test_enable(bool enable)
{
    g_audio_test_enabled = enable;
    if (enable)
    {
        g_cos_read_index   = 0;
        g_cos_pitch_factor = 256.0f / 4.0f;
        g_cos_pitch_step   = (uint16_t)g_cos_pitch_factor;
    }
}

// -----------------------------------------------------------------------------
// Callback I2S : fournit les échantillons au DMA
// -----------------------------------------------------------------------------

IRAM_ATTR bool i2s_callback(i2s_chan_handle_t handle,
                            i2s_event_data_t* event,
                            void* user_context)
{
    (void)handle;
    (void)user_context;

    g_i2s_callback_count = g_i2s_callback_count + 1;

    int16_t* samples      = (int16_t*)event->dma_buf;
    uint16_t sample_count = event->size / sizeof(int16_t);

    // --- Mode test : génération sinusoïdale depuis g_cos44100_table ---
#if AUDIO_TEST_MODE
    const bool use_test = true;
#else
    const bool use_test = g_audio_test_enabled;
#endif

    if (use_test)
    {
        while (sample_count--)
        {
            samples[0] = g_cos44100_table[g_cos_read_index >> 8];
            g_cos_read_index += g_cos_pitch_step;
            samples++;
        }
        return true;
    }

    // --- Mode normal : lecture depuis la FIFO ---
    if (fifo_used_samples() >= sample_count)
    {
        uint32_t read_offset        = g_fifo_read_index % fifo_size_in_samples();
        uint32_t contiguous_samples = fifo_size_in_samples() - read_offset;

        if (contiguous_samples >= sample_count)
        {
            // Un seul bloc contigu
            memcpy(samples,
                   &g_audio_fifo[read_offset],
                   sample_count * sizeof(int16_t));
        }
        else
        {
            // Lecture en 2 blocs (wrap de la FIFO)
            memcpy(samples,
                   &g_audio_fifo[read_offset],
                   contiguous_samples * sizeof(int16_t));
            memcpy(samples + contiguous_samples,
                   &g_audio_fifo[0],
                   (sample_count - contiguous_samples) * sizeof(int16_t));
        }

        g_fifo_read_index += sample_count;
    }
    else
    {
        // FIFO vide : on envoie du silence et on compte l’événement
        g_buffer_miss_count++;
        memset(samples, 0, sample_count * sizeof(int16_t));
    }

    return true;
}

// -----------------------------------------------------------------------------
// Configuration des callbacks I2S
// -----------------------------------------------------------------------------

static i2s_event_callbacks_t g_i2s_event_callbacks = {
    .on_recv        = nullptr,
    .on_recv_q_ovf  = nullptr,
    .on_sent        = i2s_callback,
    .on_send_q_ovf  = nullptr
};

// -----------------------------------------------------------------------------
// Initialisation de la couche I2S (canal TX, DMA, pins, format)
// -----------------------------------------------------------------------------

static int init_i2s()
{
    // Configuration du canal TX
    i2s_chan_config_t tx_channel_config{};
    tx_channel_config.id                    = I2S_NUM_AUTO;
    tx_channel_config.role                  = I2S_ROLE_MASTER;
    tx_channel_config.dma_desc_num          = 2;
    tx_channel_config.dma_frame_num         = GB_AUDIO_BUFFER_SAMPLE_COUNT;
    tx_channel_config.auto_clear_after_cb   = false;
    tx_channel_config.auto_clear_before_cb  = false;
    tx_channel_config.intr_priority         = 3;

    esp_err_t err = i2s_new_channel(&tx_channel_config, &g_i2s_tx_channel, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(AUDIO_TAG, "i2s_new_channel failed: %d", err);
        return -1;
    }

    // Configuration standard I2S (horloges, slots, GPIO)
    i2s_std_config_t tx_std_config{};

    tx_std_config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(GB_AUDIO_SAMPLE_RATE);

    tx_std_config.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_MONO
    );
    tx_std_config.slot_cfg.bit_shift = true; // alignement MSB

    tx_std_config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
#ifdef USE_MCLK_ON_GPIO42
    tx_std_config.gpio_cfg.mclk = GPIO_NUM_42;
#endif
    tx_std_config.gpio_cfg.bclk = I2S_PIN_BCLK;
    tx_std_config.gpio_cfg.ws   = I2S_PIN_WS;
    tx_std_config.gpio_cfg.dout = I2S_PIN_DOUT;
    tx_std_config.gpio_cfg.din  = I2S_GPIO_UNUSED;
    tx_std_config.gpio_cfg.invert_flags.mclk_inv = false;
    tx_std_config.gpio_cfg.invert_flags.bclk_inv = false;
    tx_std_config.gpio_cfg.invert_flags.ws_inv   = false;

    err = i2s_channel_init_std_mode(g_i2s_tx_channel, &tx_std_config);
    if (err != ESP_OK) {
        ESP_LOGE(AUDIO_TAG, "i2s_channel_init_std_mode failed: %d", err);
        return -1;
    }

    // Enregistrement du callback et activation du canal
    i2s_channel_register_event_callback(g_i2s_tx_channel,
                                        &g_i2s_event_callbacks,
                                        nullptr);
    err = i2s_channel_enable(g_i2s_tx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(AUDIO_TAG, "i2s_channel_enable failed: %d", err);
        return -1;
    }

    return 0;
}

// -----------------------------------------------------------------------------
// Configuration du codec TAS2505 (via expander / I2C)
// -----------------------------------------------------------------------------

static void audio_configure_tas2505()
{
    // Sélection de la page 0
    audio_amp_write(AUDIO_AMP_REG_PAGE, 0);
    // Reset logiciel
    audio_amp_write(AUDIO_AMP_SOFT_RESET, 1);

    // Page 1 : LDO + level shifters
    audio_amp_write(AUDIO_AMP_REG_PAGE, 1);
    audio_amp_write(AUDIO_AMP_P1_LDO_CTRL, 0);

    // Retour page 0
    audio_amp_write(AUDIO_AMP_REG_PAGE, 0);

#ifdef USE_MCLK_ON_GPIO42
    audio_amp_write(AUDIO_AMP_P0_CLK_SETTING1, 3);
#else
    audio_amp_write(AUDIO_AMP_P0_CLK_SETTING1, 4 + 3);
#endif

#ifdef USE_MCLK_ON_GPIO42
    audio_amp_write(AUDIO_AMP_P0_CLK_SETTING2, 0x91);
#else
    audio_amp_write(AUDIO_AMP_P0_CLK_SETTING2, 0x93);
#endif

    // PLL : J = 20, D = 0
    audio_amp_write(6, 20);
    audio_amp_write(7, 0x00);
    audio_amp_write(8, 0x00);

    delay_ms(15); // temps de lock PLL

    // NDAC, MDAC, OSR
    audio_amp_write(0x0B, 0x84); // NDAC = 4
    audio_amp_write(0x0C, 0x82); // MDAC = 2
    audio_amp_write(0x0D, 0x00); // DOSR high
    audio_amp_write(0x0E, 0x80); // DOSR low = 128

    // Interface I2S 16 bits, BCLK/WCLK entrées
    audio_amp_write(AUDIO_AMP_AIS_REG1, 0x00);
    audio_amp_write(0x1C, 0x00);
    audio_amp_write(0x3C, 0x02); // PRB #2, mono routing

    // DAC ON, gain 0 dB, non muté
    audio_amp_write(AUDIO_AMP_REG_PAGE, 0);
    audio_amp_write(0x3F, 0x90); // DAC ON
    audio_amp_write(0x41, 0x00); // gain 0 dB
    audio_amp_write(0x40, 0x04); // volume non muté

    // Page 1 : référence, mixer, HP et SPK
    audio_amp_write(AUDIO_AMP_REG_PAGE, 1);
    audio_amp_write(0x01, 0x10); // Master ref on
    audio_amp_write(0x0A, 0x00); // CM 0.9 V
    audio_amp_write(0x0C, 0x04); // mixer HP
    audio_amp_write(0x16, 0x00); // HP 0 dB
    audio_amp_write(0x18, 0x00);
    audio_amp_write(0x19, 0x00);
    audio_amp_write(0x09, 0x20); // HP power up
    audio_amp_write(0x10, 0x00); // HP unmute

    // Volume SPK
    audio_amp_write(AUDIO_AMP_P1_SPK_VOL, 0);        // 0 dB att
    audio_amp_write(AUDIO_AMP_P1_SPK_AMP_VOL, 0x20); // +12 dB

    // SPK ON
    audio_amp_write(AUDIO_AMP_REG_PAGE, 1);
    audio_amp_write(AUDIO_AMP_P1_SPK_AMP, 0x02);
}

// -----------------------------------------------------------------------------
// Initialisation globale de l’audio
// -----------------------------------------------------------------------------

int audio_init()
{
    printf("audio_init()\n");

    // ---------------------------------------------------------
    // 1) Pitch pour le mode test cos44100
    // ---------------------------------------------------------
    g_cos_pitch_factor = 256.0f / 4.0f;
    g_cos_pitch_step   = (uint16_t)g_cos_pitch_factor;

    // ---------------------------------------------------------
    // 2) Reset matériel de l’ampli
    // ---------------------------------------------------------
    expander_audio_amplifier_reset(0);
    delay_ms(100);
    expander_audio_amplifier_reset(1);
    delay_ms(100);

    // ---------------------------------------------------------
    // 3) Initialisation I2S
    // ---------------------------------------------------------
    if (init_i2s() != 0) {
        ESP_LOGE(AUDIO_TAG, "init_i2s failed");
        return -1;
    }

    // ---------------------------------------------------------
    // 4) Configuration TAS2505
    // ---------------------------------------------------------
    audio_configure_tas2505();

    // ---------------------------------------------------------
    // 5) Initialisation du mixeur et des pistes
    // ---------------------------------------------------------
    s_audio_player.master_volume = g_audio_settings.master_volume / 255.0f;

	// les réglages reprennent désormais les préférences utilisateur
    /* g_track_tone.volume  = 0.8f;
    g_track_noise.volume = 0.6f;
    g_track_wav.volume   = 1.0f; */

    g_track_wav.pitch        = 1.0f;
    g_track_wav.target_pitch = 1.0f;
    g_track_wav.pitch_smooth = 0.15f;

    // Ordre des pistes dans TON moteur
    s_audio_player.add_track(&g_track_tone);
    s_audio_player.add_track(&g_track_noise);
    s_audio_player.add_track(&g_track_wav);
    s_audio_player.add_track(&g_track_sfx);

    // ---------------------------------------------------------
    // 6) Préchargement SFX (sécurisé)
    // ---------------------------------------------------------
    // La SD peut être lente → on laisse 200 ms
    vTaskDelay(pdMS_TO_TICKS(200));

    printf("audio_init: preload SFX...\n");
    sfx_cache_preload_all();   

    // ---------------------------------------------------------
    // 7) Mode test audio (sinus)
    // ---------------------------------------------------------
	#if AUDIO_TEST_MODE
		audio_test_enable(true);
	#else
		audio_test_enable(false);
	#endif
	
	xTaskCreatePinnedToCore(
		audio_task,
		"audio_task",
		8192,
		nullptr,
		5,          // priorité haute
		nullptr,
		0            // core 0 (évite le core 1 où tourne WiFi/BT)
	);


    printf("audio_init: OK\n");
    return 0;
}


// -----------------------------------------------------------------------------
// Lecture de fichiers WAV (PCM 16 bits mono, 44.1 kHz) via audio_track_wav
// -----------------------------------------------------------------------------

struct WavHeader {
    char     riff[4];        // "RIFF"
    uint32_t chunkSize;
    char     wave[4];        // "WAVE"
    char     fmt[4];         // "fmt "
    uint32_t subchunk1Size;  // 16 pour PCM
    uint16_t audioFormat;    // 1 = PCM
    uint16_t numChannels;    // 1 = mono, 2 = stéréo
    uint32_t sampleRate;     // ex: 44100
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;  // ex: 16
    char     data[4];        // "data"
    uint32_t dataSize;
};

void audio_play_wav(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Impossible d’ouvrir %s\n", path);
        return;
    }

    WavHeader header{};
    if (fread(&header, sizeof(WavHeader), 1, f) != 1) {
        printf("Lecture entête WAV échouée\n");
        fclose(f);
        return;
    }

    // Vérifications de base
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        printf("Fichier %s n’est pas un WAV valide\n", path);
        fclose(f);
        return;
    }
    if (header.audioFormat != 1) {
        printf("Format non PCM (%u)\n", (unsigned)header.audioFormat);
        fclose(f);
        return;
    }
    if (header.numChannels != 1) {
        printf("Seulement mono supporté (channels=%u)\n", (unsigned)header.numChannels);
        fclose(f);
        return;
    }
    if (header.bitsPerSample != 16) {
        printf("Seulement 16 bits supportés (%u)\n", (unsigned)header.bitsPerSample);
        fclose(f);
        return;
    }
    if (header.sampleRate != GB_AUDIO_SAMPLE_RATE) {
        printf("SampleRate %u non supporté (attendu %u)\n",
               (unsigned)header.sampleRate,
               (unsigned)GB_AUDIO_SAMPLE_RATE);
        fclose(f);
        return;
    }

    // Nombre total d’échantillons 16 bits
    uint32_t total_samples = header.dataSize / sizeof(int16_t);

    // Lancer le streaming WAV sur la piste dédiée
    g_track_wav.start(f, total_samples);

    // Reset du pitch pour ce son
    g_track_wav.pitch        = 1.0f;
    g_track_wav.target_pitch = 1.0f;
}


// -----------------------------------------------------------------------------
// Mise à jour du mixeur (à appeler dans la boucle principale)
// -----------------------------------------------------------------------------

void audio_update(void)
{
    int16_t mix_sfx[GB_AUDIO_BUFFER_SAMPLE_COUNT];
    int16_t mix_music[GB_AUDIO_BUFFER_SAMPLE_COUNT];

    // 1) Mixeur interne (WAV + SFX)
    s_audio_player.mix(mix_sfx, GB_AUDIO_BUFFER_SAMPLE_COUNT);

    // 2) Volume SFX
    for (int i = 0; i < GB_AUDIO_BUFFER_SAMPLE_COUNT; i++) {
        mix_sfx[i] = (mix_sfx[i] * g_audio_settings.sfx_volume) >> 8;
	}	

	// 3) Musique PMF dans un buffer séparé
	memset(mix_music, 0, sizeof(mix_music));
	if (g_audio_settings.music_enabled){
		audioPMF.render(mix_music, GB_AUDIO_BUFFER_SAMPLE_COUNT);
	} else {
		audioPMF.stop();
	}
	
	// 3.B Lissage anti pops and crashs
	static int16_t prev_sample = 0;
	static float lp = 0.0f;

	const int music_threshold = 26000;
	const float alpha = 0.15f;
	const float smooth = 0.05f;

	for (int i = 0; i < GB_AUDIO_BUFFER_SAMPLE_COUNT; i++) {

		int32_t s = mix_music[i];

		// 1) Smoothing (5 %)
		s = (prev_sample * smooth) + (s * (1.0f - smooth));

		// 2) Mini-limiteur musique
		if (s > music_threshold) s = music_threshold;
		if (s < -music_threshold) s = -music_threshold;

		// 3) Low-pass léger
		lp = lp + alpha * ((float)s - lp);
		int16_t filtered = (int16_t)lp;

		// 4) Mise à jour du smoothing
		prev_sample = filtered;

		// 5) Écriture finale
		mix_music[i] = filtered;
	}
	
    // 4) Volume musique + ducking
	if (g_music_duck < g_music_duck_target)
		g_music_duck += g_music_duck_speed;
	else if (g_music_duck > g_music_duck_target)
		g_music_duck -= g_music_duck_speed;

    for (int i = 0; i < GB_AUDIO_BUFFER_SAMPLE_COUNT; i++) {
        int32_t s = mix_music[i];
        s = (s * g_audio_settings.music_volume) >> 8;
        s = (int32_t)(s * g_music_duck);
        mix_music[i] = (int16_t)s;
    }

    // 5) Fusion SFX + musique
    int16_t mix_final[GB_AUDIO_BUFFER_SAMPLE_COUNT];
    for (int i = 0; i < GB_AUDIO_BUFFER_SAMPLE_COUNT; i++) {
        int32_t s = (int32_t)mix_sfx[i] + (int32_t)mix_music[i];
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        mix_final[i] = (int16_t)s;
    }

    // 6) Volume master
    for (int i = 0; i < GB_AUDIO_BUFFER_SAMPLE_COUNT; i++)
        mix_final[i] = (mix_final[i] * g_audio_settings.master_volume) >> 8;

    // 7) Limiteur soft-knee
    for (int i = 0; i < GB_AUDIO_BUFFER_SAMPLE_COUNT; i++) {
        int32_t s = mix_final[i];
        const int threshold = 28000;
        const int knee = 2000;

        if (abs(s) > threshold) {
            int sign = (s >= 0) ? 1 : -1;
            int excess = abs(s) - threshold;
            float ratio = 1.0f - (float)excess / knee;
            if (ratio < 0.0f) ratio = 0.0f;
            s = threshold + (int)(excess * ratio);
            s *= sign;
        }

        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;

        mix_final[i] = (int16_t)s;
    }

    // 8) Envoi FIFO
    audio_push_buffer(mix_final);
}


// -----------------------------------------------------------------------------
// Fonctions utilitaires pour Pac-Man
// -----------------------------------------------------------------------------

void audio_play_pacgomme()
{
    const int16_t* data;
    uint32_t len;
    if (sfx_cache_load("/sdcard/PAKAMAN/Sons/PACGOMME.wav", &data, &len))
        g_track_sfx.play(data, len, 20, 1.0f, 1.0f);

    g_music_duck_target = 0.80f;
}


void audio_play_power()
{
    const int16_t* data;
    uint32_t len;
    if (sfx_cache_load("/sdcard/PAKAMAN/Sons/BONUS.wav", &data, &len))
        g_track_sfx.play(data, len, 60, 1.0f, 1.0f);

    g_music_duck_target = 0.50f;
}


void audio_play_eatghost()
{
    const int16_t* data;
    uint32_t len;
    if (sfx_cache_load("/sdcard/PAKAMAN/Sons/G_EATEN.wav", &data, &len))
        g_track_sfx.play(data, len, 80, 1.0f, 1.0f);

    g_music_duck_target = 0.35f; // baisse forte
}


void audio_play_death(void)
{
    if (!g_audio_settings.music_enabled)
    {
        audioPMF.stop();   // <-- AJOUT
    }

    audio_play_wav("/sdcard/PAKAMAN/Sons/DEATH.wav");
}


void audio_play_begin(void)
{
    if (!g_audio_settings.music_enabled)
    {
        audioPMF.stop();   // <-- AJOUT
    }

    audio_play_wav("/sdcard/PAKAMAN/Sons/BEGIN.wav");
}


// fonction de confort
bool audio_wav_is_playing(void)
{
    // Le gros fichiers WAV (DEATH, BEGIN, etc.) passent par g_track_wav
    return g_track_wav.is_active();
}


void audio_sfx_play(const char* path, int priority)
{
    const int16_t* data = nullptr;
    uint32_t len = 0;

    if (!sfx_cache_load(path, &data, &len))
        return;

    // Volume et pitch neutres, priorité passée en paramètre
    g_track_sfx.play(data, len, priority, 1.0f, 1.0f);
}



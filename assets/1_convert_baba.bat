pmf_converter.exe -i WF-DRAGO.XM -o WF-DRAGO.pmf -c -ch 4
xxd -i WF-DRAGO.pmf > WF-DRAGO.h

pmf_converter.exe -i MISTHART.XM -o MISTHART.pmf -c -ch 4
xxd -i MISTHART.pmf > MISTHART.h

pmf_converter.exe -i baba_cave_newer_short.it -o baba_cave_newer_short.pmf -c -ch 4
xxd -i baba_cave_newer_short.pmf > baba_cave_newer_short.h

pmf_converter.exe -i WF-MAGES.XM -o WF-MAGES.pmf -c -ch 4
xxd -i WF-MAGES.pmf > WF-MAGES.h

pmf_converter.exe -i CRYSTAL.XM -o CRYSTAL.pmf -c -ch 4
xxd -i CRYSTAL.pmf > CRYSTAL.h
pause
	
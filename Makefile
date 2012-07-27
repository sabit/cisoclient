CC=gcc

all:
	${CC} -g -o cisoclient ini.c cisoclient.c -I../Oscar-ISO8583/ -lcprops -L../Oscar-ISO8583/ -liso8583

# GCC        = cc -Od -Weverything -std=c++11
# GCC        = cc -Ox -Weverything -std=c++11
GCC        = cc

ASG        = proj2
MKFILE     = makefile
SOURCES1   = cpu.c
SOURCES2   = io.c
HEADERS    =
OBJECTS1   = ${SOURCES1:.c=.o}
OBJECTS2   = ${SOURCES2:.c=.o}
EXECBIN1   = cpu
EXECBIN2   = io
SRCFILES   = ${HEADERS} ${SOURCES1} ${SOURCES2} ${MKFILE}
OTHERS     = copynew config.h main.c schedproc.h schedule.c

all : ${EXECBIN1} ${EXECBIN2}

${EXECBIN1} : ${OBJECTS1}
	${GCC} -o ${EXECBIN1} ${OBJECTS1}

${EXECBIN2} : ${OBJECTS2}
	${GCC} -o ${EXECBIN2} ${OBJECTS2}

%.o : %.c
	${GCC} -c $<

clean :
	- rm ${OBJECTS1}
	- rm ${OBJECTS2}

spotless : clean
	- rm ${EXECBIN1}
	- rm ${EXECBIN2}
	- rm test.txt
	- rm ${ASG}.tar ${ASG}.tgz

zip:
	tar -c -f ${ASG}.tar README.txt design.pdf ${SRCFILES} ${OTHERS}
	gzip -c ${ASG}.tar > ${ASG}.tgz
	rm ${ASG}.tar

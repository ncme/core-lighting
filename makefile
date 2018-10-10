CC = gcc
RM = rm -vf
CFLAGS = -g -O2 -Wall -Werror -D_GNU_SOURCE -DWITH_POSIX -std=gnu99

IDIR = ./lib/cl/include
_DEPS = json-helper.h linkobj.h formobj.h embdobj.h resobj.h content.h coap-helper.h core-lighting.h bbreg.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

ODIR=./lib/cl
_OBJ = json-helper.o linkobj.o formobj.o embdobj.o resobj.o content.o coap-helper.o core-lighting.o bbreg.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

PROGS = lightBulb bulletinBoard LRC configurator
LIBRARIES = -lcoap-1 -l:libjson-c.so.3
INCLUDE = -I /usr/local/include/coap/ -I /usr/local/include/json-c/ -I ./lib/cl/include/

.PHONY:
	clean

all: $(PROGS)

$(ODIR)/%.o: $(ODIR)/%.c $(DEPS)
	$(CC) -c $< $(INCLUDE) $(LIBRARIES) $(CFLAGS) -o $@
	
$(PROGS): $(OBJ)
	$(CC) $@.c $^ $(INCLUDE) $(LIBRARIES) $(CFLAGS) -o $@
	
clean:
	$(RM) $(ODIR)/*.o $(PROGS)

show:
	echo $(DEPS)

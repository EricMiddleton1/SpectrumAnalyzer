#Compiler/Linker
CC = g++
LD = g++

#Flags
CFLAGS = -std=c++14 -Wall -pedantic -Wextra
LDFLAGS = -std=c++14 -Wall -pedantic -Wextra
LIBS = -lboost_system -lpthread -lportaudio -lfftw3 -lX11

#Extensions
HEADER = .hpp
SOURCE = .cpp
BINARY = .o

#Directories
SRCDIR = src/
OBJDIR = obj/
DIRLIST = $(SRCDIR)

#Final executable name
EXE = SpectrumAnalyzer

#Generate list of source headers with extensions
HEADERS = $(foreach DIR, $(DIRLIST), $(wildcard $(DIR)*$(HEADER)))
SOURCES = $(foreach DIR, $(DIRLIST), $(wildcard $(DIR)*$(SOURCE)))
OBJECTS = $(addprefix $(OBJDIR), $(addsuffix $(BINARY), $(notdir $(basename $(SOURCES)))))
INCLUDE = $(foreach DIR, $(DIRLIST), -I$(DIR))

all: $(EXE)

clean:
	rm -rf $(EXE) $(OBJDIR)

$(EXE):	$(OBJECTS)
				$(CC) $(CFLAGS) $(OBJECTS) -o $(EXE) $(LDFLAGS) $(LIBS)

force: clean $(EXE)

$(OBJDIR)%$(BINARY):	$(SRCDIR)%$(SOURCE) $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< $(LDFLAGS) -o $@

$(OBJDIR):
	mkdir $@

depend:
	gccmakedep -- $(LDFLAGS) -- $(SOURCES) $(HEADERS)

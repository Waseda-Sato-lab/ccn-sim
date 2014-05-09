CXX=g++
RM=rm -f
LDLIBS=-lboost_program_options

SRCS=content-size-generator.cc
OBJS=$(subst .cc,.o,$(SRCS))

all: content-size-generator

content-size-generator: $(OBJS)
	g++ -o content-size-generator $(OBJS) $(LDLIBS) 

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) -MM $^>>./.depend;

clean:
	$(RM) $(OBJS)
	$(RM) content-size-generator

dist-clean: clean
	$(RM) *~ .dependtool

include .depend

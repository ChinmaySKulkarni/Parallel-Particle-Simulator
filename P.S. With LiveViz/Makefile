CHARMC=../../../../../bin/charmc $(OPTS)

OBJS = distribution.o

all: distribution

distribution: $(OBJS)
	$(CHARMC) -language charm++ -module liveViz -o distribution $(OBJS)

proj: $(OBJS)
	$(CHARMC) -language charm++ -module liveViz -tracemode projections -o distribution.prj $(OBJS)

distribution.decl.h: distribution.ci
	$(CHARMC)  distribution.ci

clean:
	rm -f *.decl.h *.def.h *.log *.prj.sts *.prj.projrc conv-host *.o distribution charmrun distribution.prj

distribution.o: distribution.C distribution.decl.h
	$(CHARMC) -c distribution.C



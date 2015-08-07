CHARMC=../../../../../bin/charmc $(OPTS)

OBJS = load_balancing.o

all: load_balancing

load_balancing: $(OBJS)
	$(CHARMC) -g -language charm++  -o load_balancing $(OBJS)

proj: $(OBJS)
	$(CHARMC) -language charm++  -tracemode projections -o load_balancing.prj $(OBJS)

load_balancing.decl.h: load_balancing.ci
	$(CHARMC)  load_balancing.ci

clean:
	rm -f *.decl.h *.def.h *.log *.prj.sts *.prj.projrc conv-host *.o load_balancing charmrun load_balancing.prj

load_balancing.o: load_balancing.C load_balancing.decl.h
	$(CHARMC) -g -c load_balancing.C



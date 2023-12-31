SAMPLES=richards.ts richards-typed.ts splay.ts splay-typed.ts raytrace.ts raytrace-typed.ts navier-stokes.ts navier-stokes-typed.ts crypto.ts crypto-typed.ts
BENCHMARKS=splay splay-typed navier-stokes navier-stokes-typed richards richards-typed raytrace raytrace-typed crypto crypto-typed

#splayTypedStruct

benchall: $(addsuffix .bench.js, $(BENCHMARKS)) $(addsuffix .benchsafe.js, $(BENCHMARKS)) $(addsuffix .benchsafeopt.js, $(BENCHMARKS)) $(addsuffix .benchweak.js, $(BENCHMARKS)) $(addsuffix .benchtsstar.js, $(BENCHMARKS))

processbm: processbm.js
	@make benchall > octanebench 2>&1
	@node $^
	@m4 template.html > octane.html

processbm.js: processbm.ts
	@node ../../bin/tsc.js $^

wc: 
	wc -l $(SAMPLES)

playground:
	rm $(PGEXAMPLES)/*
	cp $(SAMPLES) $(PGEXAMPLES)

bm: jsonparser richards splay raytrace
	node run-octane.js

raytrace: raytrace.raw.js raytrace.safe.js raytrace-typed.safe.js

splay: splay.raw.js splay.safe.js splay-typed.safe.js

richards: richards.raw.js richards.safe.js richards-typed.raw.js richards-typed.raw.js richards-typed.safe.js

jsonparser: jsonparser.raw.js jsonparser.safe.js jsonparser.sec.js 

hand.%.time: jsonparser.handedit.%.js
	cat $^ testjson.js > $^.test
	time -f '%e secs' node $^.test

old: jsonparser.old.js
	time -f '%e secs' node $^

%.time: jsonparser.%.js
	time -f '%e secs' node $^

%.test.js: %.js testjson.js
	cat $^ >> $@

%.debug.js: ../../../panos/TypeScript/built/local/tsc.js %.ts 
	node $< --target ES5 --debug --outFile $@ --muteWarnings $*.ts

%.sec.js: %.ts ../../built/local/tsc.js boot.ts %.ts.test
	cat $< | awk 'sub (/VERSION/,"SEC")||1' > tmp.$<
	cat $<.test | awk '{gsub (/VERSION/,"SEC");print}' > tmp.$<.test
	node ../../built/local/tsc.js --target ES5 --secure --outFile $@ tmp.$<
	echo "var version = 'SEC';" >> $@
	cat tmp.$<.test >> $@

ifeq ($(NOLIB), true)
override OTHERFLAGS=--noLib
endif

mods: 
	node ../../built/local/tsc.js --target ES5 --safe ../node/node.d.ts mods.ts

#SAFEMODE_FLAGS=--relaxNullChecks --nowarn 7051 --nowarn 7083  --nowarn 7047 --nowarn 7081 --nowarn 7061 --nowarn 7087 --nowarn 7052 --nowarn 7058 --nowarn 7038 --generics --safe --noGetters
SAFEMODE_FLAGS=--generateContracts

%.js: %.ts ../../built/local/tsc.js ../../built/local/lib.d.ts
	node $(PROFILE) ../../built/local/tsc.js $(OTHERFLAGS) -t ES5 --outFile $@ $<

%.safe.js: %.ts ../../built/local/tsc.js ../../built/local/lib.d.ts
	node $(PROFILE) ../../built/local/tsc.js $(OTHERFLAGS) $(SAFEMODE_FLAGS) -t ES5 --outFile $@ $<

%.safeopt.js: %.ts ../../built/local/tsc.js ../../built/local/lib.d.ts
	node $(PROFILE) ../../built/local/tsc.js $(OTHERFLAGS) $(SAFEMODE_FLAGS) --optimizePure -t ES5 --outFile $@ $<

%.weak.js: %.ts ../../built/local/tsc.js ../../built/local/lib.d.ts
	node $(PROFILE) ../../built/local/tsc.js $(OTHERFLAGS) $(SAFEMODE_FLAGS) --weakMaps -t ES5 --outFile $@ $<

%.tsstar.js: %.ts ../../built/local/tsc.js ../../built/local/lib.d.ts
	node $(PROFILE) ../../built/local/tsc.js $(OTHERFLAGS) $(SAFEMODE_FLAGS) --tsstarTagging  -t ES5 --outFile $@ $<

prof: ../../built/local/tsc-safe.js ../../built/local/lib.d.ts
	node -prof ../../built/local/tsc-safe.js $(OTHERFLAGS) $(SAFEMODE_FLAGS) --safe --generics -t ES5 --outFile $@ deltablue.ts crypto.ts


%.raw.js: %.ts ../../built/local/tsc.js boot.ts 
	node ../../built/local/tsc.js --target ES5 --outFile $@ $<

%.test.js: %.safe.js
	node $^

%.default.js: %.ts ../../built/local/lib.d.ts rt.ts %.ts.test
	node $(PROFILE) ../../bin/tsc.js $(OTHERFLAGS) -t ES5 --outFile $@ $<

%.bench.js: %.js
	cat $< > benchmark.$<
	echo -n >> benchmark.$<
	cat benchmark.js >> benchmark.$<
	#node benchmark.$<

%.benchsafe.js: %.safe.js
	cat $< > benchmark.$<
	echo -n >> benchmark.$<
	cat benchmark.js >> benchmark.$<
	#node benchmark.$<

%.benchsafeopt.js: %.safeopt.js
	cat $< > benchmark.$<
	echo -n >> benchmark.$<
	cat benchmark.js >> benchmark.$<
	#node benchmark.$<

%.benchweak.js: %.weak.js
	cat $< > benchmark.$<
	echo -n >> benchmark.$<
	cat benchmark.js >> benchmark.$<
	#node benchmark.$<

%.benchtsstar.js: %.tsstar.js
	cat $< > benchmark.$<
	echo -n >> benchmark.$<
	cat benchmark.js >> benchmark.$<
	#node benchmark.$<

%.benchdef.js: %.default.js
	cat $< > benchmark.$<
	echo -n >> benchmark.$<
	cat benchmark.js >> benchmark.$<
	#node benchmark.$<

profile-safe: 
	node --prof ../../built/local/tsc.js $(OTHERFLAGS) --nowarn 7081 --knownWarnings triage --generics --safe --noRuntimeChecks -t ES5 -removeComments id.ts micro.ts classes.ts jsonparser.ts richards-typed.ts splay-typed.ts raytrace-typed.ts navier-stokes-typed.ts crypto-typed.ts

mylib:
	node ../../built/local/tsc.js --noLib --safe --noRuntimeChecks -t ES5 -noImplicitAny -removeComments mylib.d.ts

%.ts.test: %.ts
	echo "/* no test yet */" > $@

clean:
	rm -f *.raw.js *.sec.js *.safe.js *.safer.js benchmark.*.js benchmark.*.safe.js benchmark.*.safeopt.js benchmark.*.weak.js benchmark.*.tsstar.js data.html slowdowns.html octane.html octanebench processbm.js $(addsuffix .js, $(BENCHMARKS)) *~

all: worker worker_runner

worker_runner: storage
	$(MAKE) -f Makefile.worker_runner USE_PGXS=1 install

worker: storage
	$(MAKE) -f Makefile.worker USE_PGXS=1 install

storage:
	$(MAKE) -f Makefile.storage USE_PGXS=1 install

clean: clean_worker clean_worker_runner storage

clean_worker:
	rm -f worker.so worker.o worker.bc

clean_worker_runner:
	rm -f worker_runner.so worker_runner.o worker_runner.bc

clean_storage:
	rm -f storage.o

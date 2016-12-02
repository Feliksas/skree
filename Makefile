CXX = $(HOME)/llvm/llvm_cmake_build/bin/clang++

CXXFLAGS = -I $(HOME)/llvm/llvm_cmake_build/lib/clang/4.0.0/include/ \
-I /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../include/c++/v1 \
-I /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include \
-I /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/usr/include \
-I /usr/local/include/ \
-I contrib-build/usr/local/include \
-I contrib-build/usr/include \
-I contrib-build/include \
-std=c++11 -fstack-protector-all -Wno-expansion-to-defined \
-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free

LDFLAGS = -L /usr/local/lib \
-L contrib-build/usr/local/lib \
-L contrib-build/usr/lib \
-L contrib-build/lib \
-fsanitize=address,undefined,safe-stack \
# -Wl,--export-dynamic \

OBJS = main.o src/skree/actions/n.o src/skree/actions/c.o src/skree/actions/e.o src/skree/actions/h.o src/skree/actions/i.o src/skree/actions/l.o src/skree/actions/r.o src/skree/actions/w.o src/skree/actions/x.o src/skree/base/action.o src/skree/base/pending_read.o src/skree/base/pending_write.o src/skree/base/worker.o src/skree/client.o src/skree/db_wrapper.o src/skree/meta/opcodes.o src/skree/pending_reads/replication/ping_task.o src/skree/pending_reads/replication/propose_self.o src/skree/pending_reads/replication.o src/skree/server.o src/skree/utils/misc.o src/skree/workers/client.o src/skree/workers/discovery.o src/skree/workers/replication.o src/skree/workers/synchronization.o src/skree/workers/statistics.o src/skree/queue_db.o src/skree/workers/processor.o src/skree/workers/cleanup.o src/skree/meta/states.o src/skree/utils/string_sequence.o
# OBJS = src/skree/queue_db.o

LIBS = -lpthread -lstdc++ -lm -lyaml-cpp -ldl -lwiredtiger -lprofiler -ltcmalloc

TARGET = build/skree

$(TARGET): $(OBJS)
	mkdir -p build
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS) $(LDFLAGS)
	rm -f $(OBJS)

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

clean_all: clean_contrib clean

install:
	mkdir -p /usr/bin
	mv $(TARGET) /usr/bin/

contrib: contrib_yaml_cpp contrib_gperftools contrib_wiredtiger

contrib_yaml_cpp:
	mkdir -p contrib-build
	cd contrib/yaml-cpp && cmake . && make -j 16 && make DESTDIR=../../contrib-build install

contrib_gperftools:
	mkdir -p contrib-build
	cd contrib/gperftools && ./autogen.sh && ./configure --prefix=`pwd`/../../contrib-build && make -j 16 && make install

contrib_wiredtiger:
	mkdir -p contrib-build
	cd contrib/wiredtiger && ./autogen.sh && ./configure --prefix=`pwd`/../../contrib-build && make -j 16 && make install

clean_contrib: clean_contrib_yaml_cpp clean_contrib_gperftools clean_contrib_wiredtiger
	rm -rf contrib-build

clean_contrib_yaml_cpp:
	cd contrib/yaml-cpp && make DESTDIR=../../contrib-build uninstall && make clean ||:
	rm contrib/yaml-cpp/CMakeCache.txt ||:

clean_contrib_gperftools:
	cd contrib/gperftools && make uninstall && make clean ||:

clean_contrib_wiredtiger:
	cd contrib/wiredtiger && make uninstall && make clean ||:

all:
	@mkdir -p build
	@cd build && cmake .. && make -j$(nproc)

install:
	@cd build && sudo make install

clean:
	@rm -rf build .lucis/build

.PHONY: all install clean

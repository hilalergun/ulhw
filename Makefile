all: install

install: build
	@cp data/* /run/user/1000/

make install_debs:
	@sudo dpkg -i deps/*.deb

.PHONY: build
build:
	@mkdir build
	@(cd build && cmake .. && make -j4 && ln -s ulhw ulhwcli)

clean:
	@rm -rf build

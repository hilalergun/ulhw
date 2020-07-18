all: install

install: build
	@mkdir p /run/user/1000/
	@cp data/* /run/user/1000/

make install_debs:
	@dpkg -i deps/*.deb

.PHONY: build
build:
	@mkdir -p build
	@(cd build && cmake .. && make -j4 && ln -s ulhw ulhwcli)

clean:
	@rm -rf build

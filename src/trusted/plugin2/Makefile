CHROME_CHECKOUT=/Projects/chrome
NACL_CHECKOUT=/Projects/native_client

BUILT_CHROME=$(CHROME_CHECKOUT)/src/xcodebuild/Debug/Chromium.app/Contents/MacOS/Chromium

NACL_PLUGIN=$(NACL_CHECKOUT)/native_client/scons-out/dbg-mac-x86-32/staging/ppNaClPlugin2

PLUGIN_TYPE=application/x-ppapi-example
EXAMPLE_HTML=$(CHROME_CHECKOUT)/src/ppapi/example/example.html

# This could also be "."
PLUGIN2_DIR=$(NACL_CHECKOUT)/native_client/src/trusted/plugin2

NEXE=test_nexe/hello_ppapi.nexe

all: run

patch:
	patch -d $(CHROME_CHECKOUT) -p0 -i $(PLUGIN2_DIR)/chrome.patch
	patch -d $(NACL_CHECKOUT) -p0 -i $(PLUGIN2_DIR)/nacl.patch

chrome:
	(cd $(CHROME_CHECKOUT)/src/chrome; xcodebuild)

$(NACL_PLUGIN):
	(cd $(NACL_CHECKOUT)/native_client; ./scons --mode=dbg-mac ppNaClPlugin2 sdl=none)

plugin: $(NACL_PLUGIN)

$(NEXE):
	(cd test_nexe; make)

nexe: $(NEXE)

run: $(NACL_PLUGIN) $(NEXE)
	 $(BUILT_CHROME) \
		--register-pepper-plugins="$(NACL_PLUGIN);$(PLUGIN_TYPE)" \
		--ppapi-out-of-process \
		$(EXAMPLE_HTML)

clean:
	rm $(NACL_PLUGIN)
	(cd test_nexe; make clean)
BUILD_DIR    := build
XCFW_DIR     := wrappers/swift-exif/libexif.xcframework
HEADERS_DIR  := $(BUILD_DIR)/xcfw-headers
MACOS_TARGET := 15.0

.PHONY: all clean xcframework lib

all: xcframework

lib: $(BUILD_DIR)/libexif.a

$(BUILD_DIR)/libexif.a:
	cmake -B $(BUILD_DIR) -DCMAKE_OSX_DEPLOYMENT_TARGET=$(MACOS_TARGET) -DCMAKE_OSX_ARCHITECTURES=arm64
	cmake --build $(BUILD_DIR) --target exif -j$$(sysctl -n hw.ncpu)

xcframework: $(BUILD_DIR)/libexif.a
	rm -rf $(XCFW_DIR) $(HEADERS_DIR)
	mkdir -p $(HEADERS_DIR)
	cp libexif.h $(HEADERS_DIR)/
	xcodebuild -create-xcframework \
		-library $(BUILD_DIR)/libexif.a \
		-headers $(HEADERS_DIR) \
		-output $(XCFW_DIR)
	printf 'module CLibExif {\n    header "libexif.h"\n    link "exif"\n    export *\n}\n' \
		> $(XCFW_DIR)/macos-arm64/Headers/module.modulemap
	rm -rf $(HEADERS_DIR)

clean:
	rm -rf $(BUILD_DIR) $(XCFW_DIR)

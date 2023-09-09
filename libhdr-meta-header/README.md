# libhdr-meta-plugin usage

## Setup
1. Create a new directory for the libhdr-meta-plugin


2. Create a build script(ex> Android.bp) & source code(ex> libhdr-meta-default.cpp)<br>
- Include the header lib with the following name.<br>
```
        'libhdr_meta_interface_header'
```

```
    ex)
    cc_library {
        name: "libhdr_meta_plugin_default",
        cflags: [
            "-Wno-unused-function",
            "-DLOG_TAG=\"libhdr-meta\"",
        ],
        header_libs: [
            "libhdr_meta_interface_header",
        ],
        srcs: [
            "./srcs/meta/libhdr_meta_default.cpp",
        ],
        proprietary: true,
    }
```

3. Implement the hdrMetaInterface on the source code.<br>
   Use a following template below and refer to the instructions given as comments on the hdrMetaInterface.h<br>
   If the functions return with the values other than "HDRMETAIF-ERR-NO",<br>
   the function is regarded as 'not implemented', and they will have no effect on the video playback.<br>
```
    ex)
    #include <hdrMetaInterface.h>

    class hdrMetaImplementation: public hdrMetaInterface {
        convertHDR10pMeta(void __attribute__((unused)) *in_metadata, unsigned int __attribute__((unused)) in_len,
            	    unsigned int __attribute__((unused)) targetMaxLuminance,
            	    void __attribute__((unused)) *out_metadata, unsigned int __attribute__((unused)) out_len) {
            return HDRMETAIF_ERR_MAX;
        }

        int configHDR10Tonemap(
                unsigned int __attribute__((unused)) srcMasteringLuminance,
                unsigned int __attribute__((unused)) targetMaxLuminance
                ) {
            return HDRMETAIF_ERR_MAX;
        }

        int computeHDR10Tonemap(
                double __attribute__((unused)) inX,
                double __attribute__((unused)) *outY
                ) {
            return HDRMETAIF_ERR_MAX;
        }

        int getTargetLuminance(int __attribute__((unused))curTargetLuminance, int __attribute__((unused)) *outTargetLuminance) {
            return HDRMETAIF_ERR_MAX;
        }

        void setLogLevel(int __attribute__((unused)) log_level) {}
    };

    hdrMetaInterface *hdrMetaInterface::createInstance(void) {
        return hdrMetaImplementation();
    }
```

4. Replace the meta plugin on the BoardConfig.mk with the actual name of the plugin library.

```
    ex)
    if the name of the plugin library is 'libhdr_meta_plugin_X'

        cc_library {
            name: "libhdr_meta_plugin_X",

    , then replace the following defined value(SOONG_CONFIG_libhdr_meta_plugin) on the BoardConfig.mk with it.

        SOONG_CONFIG_libhdr_meta_plugin := libhdr_meta_plugin_X
```


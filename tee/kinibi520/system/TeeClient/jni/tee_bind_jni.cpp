/*
 * Copyright (c) 2015-2017 TRUSTONIC LIMITED
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the TRUSTONIC LIMITED nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <limits.h>
#include <dlfcn.h>
#include "tee_bind_jni.h"

#include "dynamic_log.h"

class JavaClassManager {
    // Cache a reference to the class loader and resolve the classes from
    // this cached loader
    jobject     class_loader_ = nullptr;
    jmethodID   find_class_   = nullptr;
    void init(JNIEnv* jenv, const char* classPath) {
        if (class_loader_) {
            // Nothing to do, use the cached one
            return;
        }

        jclass local_class = jenv->FindClass(classPath);
        jclass local_class_obj = jenv->GetObjectClass(local_class);
        jmethodID local_class_get_loader = jenv->GetMethodID(local_class_obj,
                                                             "getClassLoader",
                                                             "()Ljava/lang/ClassLoader;");
        class_loader_ = static_cast<jobject>(
                            jenv->NewGlobalRef(
                                jenv->CallObjectMethod(local_class,
                                                       local_class_get_loader)));
        jclass system_class_loader = jenv->FindClass("java/lang/ClassLoader");
        find_class_ = static_cast<jmethodID>(
                            jenv->GetMethodID(system_class_loader,
                                              "findClass",
                                              "(Ljava/lang/String;)Ljava/lang/Class;"));
        if (jenv->ExceptionCheck()) {
            LOG_E("Exception already!");
            jenv->ExceptionDescribe();
        }
    }
    jclass load(JNIEnv* jenv, const char* classPath) {
        init(jenv, classPath);
        return static_cast<jclass>(
                    jenv->CallObjectMethod(class_loader_,
                                           find_class_,
                                           jenv->NewStringUTF(classPath)));
    }
    static JavaClassManager& getInstance() {
        static JavaClassManager manager;
        return manager;
    }
public:
    static jclass resolve(JNIEnv* jenv, const char* classPath) {
        return getInstance().load(jenv, classPath);
    }
};

struct JavaProcess::Impl {
    std::string package_name;
    std::string starter_class;
    std::string starter_class_path;
    bool        is_binded = false;
    JavaVM*     jvm = nullptr;
    JNIEnv*     jenv = nullptr;
    bool        thread_attached = false;
    jclass      tee_bind_java_class = nullptr;

    enum class BindResult {
        OK,
        OVERLOAD,
        FAILURE
    };

    Impl(
        const std::string& pn,
        const std::string& sc):
        package_name(pn), starter_class(sc) {
        starter_class_path = package_name + "/." + starter_class;
    }

    int setup() {
        if (!jvm) {
            LOG_E("No JVM");
            return -1;
        }

        JNIEnv* jenv_new;
        int getEnvStat = jvm->GetEnv(reinterpret_cast<void**>(&jenv_new), JNI_VERSION_1_6);
        if (getEnvStat == JNI_EDETACHED) {
            LOG_I("GetEnv: not attached");
            if (jvm->AttachCurrentThread((&jenv_new), NULL) != JNI_OK) {
                LOG_E("Failed to attach");
                return -1;
            }
            thread_attached = true;
        } else if (getEnvStat == JNI_EVERSION) {
            LOG_E("Version not supported");
            return -1;
        }
        // getEnvStat == JNI_OK
        if (jenv_new->ExceptionCheck()) {
            LOG_E("Exception already!");
            jenv_new->ExceptionDescribe();
        }
        if (jenv_new != jenv) {
            LOG_I("Updating Env! - Recreating Java client");
            jenv = jenv_new;
            if (tee_bind_java_class) {
                jenv->DeleteGlobalRef(tee_bind_java_class);
            }
            tee_bind_java_class = static_cast<jclass>(
                                jenv->NewGlobalRef(
                                    JavaClassManager::resolve(
                                        jenv,
                                        "com/trustonic/teeclient/TeeBind")));
        }

        return 0;
    }

    void teardown() {
        if (jvm && thread_attached) {
            jvm->DetachCurrentThread();
            thread_attached = false;
        }
    }

    int registerContext(JavaVM* jvm_in, jobject context) {
        jvm = jvm_in;

        int rc = setup();
        if (rc) {
            // Log done in setup
            return rc;
        }

        jmethodID registerContext = jenv->GetStaticMethodID(tee_bind_java_class,
                                                            "registerContext",
                                                            "(Landroid/content/Context;)V");
        if (registerContext == NULL) {
            LOG_E("Failed to get static method 'registerContext' from TeeClient");
            rc = -1;
        } else {
            jenv->CallStaticVoidMethod(tee_bind_java_class, registerContext, context);
            LOG_D("Context registered successfully, java initialization complete");
        }

        teardown();
        return rc;
    }

    void broadcastIntent(const std::string& action) {
        if (setup()) {
            // Log done in setup
            return;
        }

        jmethodID broadcastIntent = jenv->GetStaticMethodID(tee_bind_java_class,
                                                            "broadcastIntent",
                                                            "(Ljava/lang/String;)V");
        if (!broadcastIntent) {
            LOG_E("Failed to get static method 'broadcastIntent' from TeeClient");
        } else {
            jstring jAction = jenv->NewStringUTF(action.c_str());
            jenv->CallStaticVoidMethod(tee_bind_java_class, broadcastIntent, jAction);
            jenv->DeleteLocalRef(jAction);
        }

        teardown();
    }
};

JavaProcess::JavaProcess(
    const std::string& package_name,
    const std::string& starter_class):
    // Missing std::make_unique
    pimpl_(new Impl(package_name, starter_class)) {
}

JavaProcess::~JavaProcess() {
    unbind();
}

bool JavaProcess::bind(
    JavaVM* jvm,
    jobject javaContext,
    bool send_restart) {
    if (pimpl_->is_binded) {
        return true;
    }

    LOG_I("Bind service %s", pimpl_->package_name.c_str());
    if (pimpl_->registerContext(jvm, javaContext)) {
        errno = ENXIO;
        return false;
    }

    if (pimpl_->setup()) {
        // Log done in setup
        return false;
    }

    Impl::BindResult rc = Impl::BindResult::FAILURE;
    jmethodID bind = pimpl_->jenv->GetStaticMethodID(pimpl_->tee_bind_java_class,
                                                     "bind",
                                                     "(Ljava/lang/String;)I");
    if (bind == NULL) {
        LOG_E("Failed to get static method 'bind' from TeeClient");
    } else {
        jstring jStarterClass = pimpl_->jenv->NewStringUTF(pimpl_->starter_class_path.c_str());
        jint ret = pimpl_->jenv->CallStaticIntMethod(pimpl_->tee_bind_java_class,
                                                     bind, jStarterClass);
        pimpl_->jenv->DeleteLocalRef(jStarterClass);

        rc = static_cast<Impl::BindResult>(ret);
        if (rc == Impl::BindResult::FAILURE) {
            errno = ECONNREFUSED;
        } else {
            pimpl_->is_binded = true;
        }
    }

    pimpl_->teardown();
    if ((rc == Impl::BindResult::FAILURE) && send_restart) {
        // Tui Service binding has been disabled on SSMG GS7 and GS8.
        // Instead, the Client Application should do send a "restart"
        // intent
        pimpl_->broadcastIntent(pimpl_->package_name + ".action.restart");
    }

    return rc == Impl::BindResult::OK;
}

void JavaProcess::unbind() {
    if (!pimpl_->is_binded) {
        return;
    }

    LOG_I("Unbind service %s", pimpl_->package_name.c_str());
    if (pimpl_->setup()) {
        // Log done in setup
        return;
    }

    jmethodID unbind = pimpl_->jenv->GetStaticMethodID(pimpl_->tee_bind_java_class,
                                                       "unbind",
                                                       "(Ljava/lang/String;)I");
    if (unbind == NULL) {
        LOG_E("Cannot get method ID for TeeClient's \"unbind\" java method");
    } else {
        jstring jStarterClass = pimpl_->jenv->NewStringUTF(pimpl_->starter_class_path.c_str());
        jint ret = pimpl_->jenv->CallStaticIntMethod(pimpl_->tee_bind_java_class,
                                                     unbind, jStarterClass);
        pimpl_->jenv->DeleteLocalRef(jStarterClass);

        Impl::BindResult rc = static_cast<Impl::BindResult>(ret);
        if (rc == Impl::BindResult::OK) {
            pimpl_->is_binded = false;
        }
    }

    pimpl_->teardown();
}

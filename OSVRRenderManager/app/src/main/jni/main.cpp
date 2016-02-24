/*
 * Copyright (C) 2015 Sensics, Inc. and contributors
 *
 * Based on Android NDK samples, which are:
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//BEGIN_INCLUDE(all)
#include <jni.h>
#include <android/log.h>

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>

#include <boost/filesystem.hpp>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/ClientKit/InterfaceStateC.h>
#include <osvr/ClientKit/Display.h>
#include <osvr/JointClientKit/JointClientKitC.h>
#include <osvr/ClientKit/ContextC.h>
#include <osvr/ClientKit/InterfaceC.h>
#include <osvr/ClientKit/InterfaceCallbackC.h>

#include <osvr/Server/ConfigureServerFromFile.h>

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static const char gVertexShader[] =
                "uniform mat4 model;\n"
                "uniform mat4 view;\n"
                "uniform mat4 projection;\n"
                "attribute vec4 vPosition;\n"
                "attribute vec4 vColor;\n"
                "varying vec4 fragmentColor;\n"
                "void main() {\n"
                "  gl_Position = projection * view * model * vPosition;\n"
                "  fragmentColor = vColor;\n"
                "}\n";

static const char gFragmentShader[] =
        "precision mediump float;\n"
                "uniform sampler2D uTexture;\n"
                "varying vec2 texCoordinate;\n"
                "varying vec4 fragmentColor;\n"
                "void main()\n"
                "{\n"
                "    gl_FragColor = fragmentColor;\n"
                "}\n";

static GLuint gProgram;
static GLuint gvPositionHandle;
static GLuint gvColorHandle;
static GLuint gvProjectionUniformId;
static GLuint gvViewUniformId;
static GLuint gvModelUniformId;
static osvr::clientkit::DisplayConfig* gOSVRDisplayConfig;
static osvr::clientkit::ClientContext* gClientContext;
static osvr::server::ServerPtr gServer;
static int gReportNumber = 0;

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

static GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n",
                         shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

static GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");

        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");

        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

static GLuint createTexture(GLuint width, GLuint height) {
    GLuint ret;
    glGenTextures(1, &ret);
    checkGlError("glGenTextures");

    glBindTexture(GL_TEXTURE_2D, ret);
    checkGlError("glBindTexture");

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

//    // DEBUG CODE - should be passing null here, but then texture is always black.
    GLubyte *dummyBuffer = new GLubyte[width * height * 4];
    for(GLuint i = 0; i < width * height * 4; i++) {
        dummyBuffer[i] = (i % 4 ? 100 : 255);
    }

    // This dummy texture successfully makes it into the texture and renders, but subsequent
    // calls to glTexSubImage2D don't appear to do anything.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummyBuffer);
    checkGlError("glTexImage2D");
    delete[] dummyBuffer;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    checkGlError("glTexParameteri");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    checkGlError("glTexParameteri");
    return ret;
}

static void checkReturnCode(OSVR_ReturnCode returnCode, const char* msg) {
    if(returnCode != OSVR_RETURN_SUCCESS) {
        LOGI("[OSVR] OSVR method returned a failure: %s", msg);
        throw std::runtime_error(msg);
    }
}

static bool setupOSVR() {
    try {
        // On Android, the current working directory is added to the default plugin search path.
        // it also helps the server find its configuration and display files.
        boost::filesystem::current_path("/data/data/com.osvr.android.gles2sample/files");
        auto workingDirectory = boost::filesystem::current_path();
        LOGI("[OSVR] Current working directory: %s", workingDirectory.string().c_str());

        // @todo separate OSVR setup from graphics setup.
        // We sometimes get more than one onSurfaceChanged event, so these
        // checks are to prevent multiple OSVR servers being setup.
        if(!gServer) {
            std::string configName(osvr::server::getDefaultConfigFilename());
            LOGI("[OSVR] Configuring server from file %s", configName.c_str());

            gServer = osvr::server::configureServerFromFile(configName);

            LOGI("[OSVR] Starting server...");
            gServer->start();
        }

        if(!gClientContext) {
            LOGI("[OSVR] Creating ClientContext...");
            gClientContext = new osvr::clientkit::ClientContext(
                    "com.osvr.android.examples.OSVRRenderManager");

            // temporary workaround to DisplayConfig issue,
            // display sometimes fails waiting for the tree from the server.
            LOGI("[OSVR] Calling update a few times...");
            for (int i = 0; i < 10000; i++) {
                gClientContext->update();
            }

            if (!gClientContext->checkStatus()) {
                LOGI("[OSVR] Client context reported bad status.");
                return false;
            }

            gOSVRDisplayConfig = new osvr::clientkit::DisplayConfig(*gClientContext);
            if (!gOSVRDisplayConfig->valid()) {
                LOGI("[OSVR] Could not get a display config (server probably not "
                             "running or not behaving), exiting");
                return false;
            }

            LOGI("[OSVR] Got a valid display config. Waiting for the display to fully start up, including "
                         "receiving initial pose update...");
            using clock = std::chrono::system_clock;
            auto timeEnd = clock::now() + std::chrono::seconds(2);

            while (clock::now() < timeEnd && !gOSVRDisplayConfig->checkStartup()) {
                gClientContext->update();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            if (!gOSVRDisplayConfig->checkStartup()) {
                LOGI("[OSVR] Timed out waiting for display to fully start up and receive the initial pose update.");
                return false;
            }

            LOGI("[OSVR] OK, display startup status is good!");
        }

        return true;
    } catch(const std::runtime_error &ex) {
        LOGI("[OSVR] OSVR initialization failed: %s", ex.what());
        return false;
    }
}

static bool setupGraphics(int width, int height) {
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    LOGI("setupGraphics(%d, %d)", width, height);
    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram) {
        LOGE("Could not create program.");
        return false;
    }
    gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
    checkGlError("glGetAttribLocation");
    LOGI("glGetAttribLocation(\"vPosition\") = %d\n", gvPositionHandle);

    gvColorHandle = glGetAttribLocation(gProgram, "vColor");
    checkGlError("glGetAttribLocation");
    LOGI("glGetAttribLocation(\"vColor\") = %d\n", gvColorHandle);

    gvProjectionUniformId = glGetUniformLocation(gProgram, "projection");
    gvViewUniformId = glGetUniformLocation(gProgram, "view");
    gvModelUniformId = glGetUniformLocation(gProgram, "model");

    glViewport(0, 0, width, height);
    checkGlError("glViewport");

    glDisable(GL_CULL_FACE);

    return setupOSVR();
}

static const GLfloat gTriangleColors[] = {
        // white
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,

        // green
        0.0f, 0.75f, 0.0f, 1.0f,
        0.0f, 0.75f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.75f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,

        // blue
        0.0f, 0.0f, 0.75f, 1.0f,
        0.0f, 0.0f, 0.75f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.75f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,

        // green/purple
        0.0f, 0.75f, 0.75f, 1.0f,
        0.0f, 0.75f, 0.75f, 1.0f,
        0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.75f, 0.75f, 1.0f,
        0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 1.0f, 1.0f,

        // red/green
        0.75f, 0.75f, 0.0f, 1.0f,
        0.75f, 0.75f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
        0.75f, 0.75f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,

        // red/blue
        0.75f, 0.0f, 0.75f, 1.0f,
        0.75f, 0.0f, 0.75f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f,
        0.75f, 0.0f, 0.75f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f
};

static const GLfloat gTriangleTexCoordinates[] = {
        // A cube face (letters are unique vertices)
        // A--B
        // |  |
        // D--C

        // As two triangles (clockwise)
        // A B D
        // B C D

        // white
        1.0f, 0.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        0.0f, 1.0f, // C
        0.0f, 0.0f, // D

        // green
        1.0f, 0.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        0.0f, 1.0f, // C
        0.0f, 0.0f, // D

        // blue
        1.0f, 1.0f, // A
        0.0f, 1.0f, // B
        1.0f, 0.0f, // D
        0.0f, 1.0f, // B
        0.0f, 0.0f, // C
        1.0f, 0.0f, // D

        // blue-green
        1.0f, 0.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        0.0f, 1.0f, // C
        0.0f, 0.0f, // D

        // yellow
        0.0f, 0.0f, // A
        1.0f, 0.0f, // B
        0.0f, 1.0f, // D
        1.0f, 0.0f, // B
        1.0f, 1.0f, // C
        0.0f, 1.0f, // D

        // purple/magenta
        1.0f, 1.0f, // A
        0.0f, 1.0f, // B
        1.0f, 0.0f, // D
        0.0f, 1.0f, // B
        0.0f, 0.0f, // C
        1.0f, 0.0f, // D
};

static const GLfloat gTriangleVertices[] = {
        // A cube face (letters are unique vertices)
        // A--B
        // |  |
        // D--C

        // As two triangles (clockwise)
        // A B D
        // B C D

        //glNormal3f(0.0, 0.0, -1.0);
        1.0f, 1.0f, -1.0f, // A
        1.0f, -1.0f, -1.0f, // B
        -1.0f, 1.0f, -1.0f, // D
        1.0f, -1.0f, -1.0f, // B
        -1.0f, -1.0f, -1.0f, // C
        -1.0f, 1.0f, -1.0f, // D

        //glNormal3f(0.0, 0.0, 1.0);
        -1.0f, 1.0f, 1.0f, // A
        -1.0f, -1.0f, 1.0f, // B
        1.0f, 1.0f, 1.0f, // D
        -1.0f, -1.0f, 1.0f, // B
        1.0f, -1.0f, 1.0f, // C
        1.0f, 1.0f, 1.0f, // D

//        glNormal3f(0.0, -1.0, 0.0);
        1.0f, -1.0f, 1.0f, // A
        -1.0f, -1.0f, 1.0f, // B
        1.0f, -1.0f, -1.0f, // D
        -1.0f, -1.0f, 1.0f, // B
        -1.0f, -1.0f, -1.0f, // C
        1.0f, -1.0f, -1.0f, // D

//        glNormal3f(0.0, 1.0, 0.0);
        1.0f, 1.0f, 1.0f, // A
        1.0f, 1.0f, -1.0f, // B
        -1.0f, 1.0f, 1.0f, // D
        1.0f, 1.0f, -1.0f, // B
        -1.0f, 1.0f, -1.0f, // C
        -1.0f, 1.0f, 1.0f, // D

//        glNormal3f(-1.0, 0.0, 0.0);
        -1.0f, 1.0f, 1.0f, // A
        -1.0f, 1.0f, -1.0f, // B
        -1.0f, -1.0f, 1.0f, // D
        -1.0f, 1.0f, -1.0f, // B
        -1.0f, -1.0f, -1.0f, // C
        -1.0f, -1.0f, 1.0f, // D

//        glNormal3f(1.0, 0.0, 0.0);
        1.0f, -1.0f, 1.0f, // A
        1.0f, -1.0f, -1.0f, // B
        1.0f, 1.0f, 1.0f, // D
        1.0f, -1.0f, -1.0f, // B
        1.0f, 1.0f, -1.0f, // C
        1.0f, 1.0f, 1.0f // D
};

/**
 * Just the current frame in the display.
 */
static void renderFrame() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    if(gOSVRDisplayConfig != NULL && gClientContext != NULL) {
        gClientContext->update();
        //LOGI("[OSVR] got osvrDisplayConfig. iterating through displays.");
        gOSVRDisplayConfig->forEachEye([](osvr::clientkit::Eye eye) {

            /// Try retrieving the view matrix (based on eye pose) from OSVR
            GLfloat viewMat[OSVR_MATRIX_SIZE];
            eye.getViewMatrix(OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS,
                              viewMat);

            /// For each display surface seen by the given eye of the given
            /// viewer...
            eye.forEachSurface([&viewMat](osvr::clientkit::Surface surface) {
                auto viewport = surface.getRelativeViewport();
                glViewport(static_cast<GLint>(viewport.left),
                           static_cast<GLint>(viewport.bottom),
                           static_cast<GLsizei>(viewport.width),
                           static_cast<GLsizei>(viewport.height));

                /// Set the OpenGL projection matrix based on the one we
                /// computed.
                GLfloat zNear = 0.001f;
                GLfloat zFar = 10000.0f;
                GLfloat projMat[OSVR_MATRIX_SIZE];
                surface.getProjectionMatrix(
                        zNear, zFar, OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS |
                                     OSVR_MATRIX_SIGNEDZ | OSVR_MATRIX_RHINPUT,
                        projMat);

                const static GLfloat identityMat4f[16] = {
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f,
                };

                /// Call out to render our scene.
                glUseProgram(gProgram);
                checkGlError("glUseProgram");

                glUniformMatrix4fv(gvProjectionUniformId, 1, GL_FALSE, projMat);
                glUniformMatrix4fv(gvViewUniformId, 1, GL_FALSE, viewMat);
                glUniformMatrix4fv(gvModelUniformId, 1, GL_FALSE, identityMat4f);

                glVertexAttribPointer(gvPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
                checkGlError("glVertexAttribPointer");
                glEnableVertexAttribArray(gvPositionHandle);
                checkGlError("glEnableVertexAttribArray");

                glVertexAttribPointer(gvColorHandle, 4, GL_FLOAT, GL_FALSE, 0, gTriangleColors);
                checkGlError("glVertexAttribPointer");
                glEnableVertexAttribArray(gvColorHandle);
                checkGlError("glEnableVertexAttribArray");

                glDrawArrays(GL_TRIANGLES, 0, 36);
                checkGlError("glDrawArrays");
            });
        });
    }
}

static void stop() {
    LOGI("[OSVR] Shutting down...");
    if(gOSVRDisplayConfig != nullptr) {
        delete gOSVRDisplayConfig;
        gOSVRDisplayConfig = nullptr;
    }

    // is this needed? Maybe not. the display config manages the lifetime.
    if(gClientContext != nullptr) {
        delete gClientContext;
        gClientContext = nullptr;
    }

    if(gServer) {
        LOGI("[OSVR] Shutting down server...");
        gServer->stop();
        gServer.reset();
    }
}

extern "C" {
    JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_init(JNIEnv * env, jobject obj,  jint width, jint height);
    JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_step(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_stop(JNIEnv * env, jobject obj);
};

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_init(JNIEnv * env, jobject obj,  jint width, jint height)
{
    setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_step(JNIEnv * env, jobject obj)
{
    renderFrame();
}

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_stop(JNIEnv * env, jobject obj)
{
    stop();
}

//END_INCLUDE(all)

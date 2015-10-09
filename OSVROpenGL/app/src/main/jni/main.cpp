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
#include <osvr/ClientKit/ImagingC.h>

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

static const char gVertexShader[] =
                "uniform mat4 model;\n"
                "uniform mat4 view;\n"
                "uniform mat4 projection;\n"
                "attribute vec4 vPosition;\n"
                "attribute vec4 vColor;\n"
                "attribute vec2 vTexCoordinate;\n"
                "varying vec2 texCoordinate;\n"
                "varying vec4 fragmentColor;\n"
                "void main() {\n"
                "  gl_Position = projection * view * model * vPosition;\n"
                "  fragmentColor = vColor;\n"
                "  texCoordinate = vTexCoordinate;\n"
                "}\n";

static const char gFragmentShader[] =
        "precision mediump float;\n"
                "uniform sampler2D uTexture;\n"
                "varying vec2 texCoordinate;\n"
                "varying vec4 fragmentColor;\n"
                "void main()\n"
                "{\n"
                "    gl_FragColor = fragmentColor * texture2D(uTexture, texCoordinate);\n"
                //"    gl_FragColor = texture2D(uTexture, texCoordinate);\n"
                "}\n";

GLuint loadShader(GLenum shaderType, const char* pSource) {
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

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
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

GLuint gProgram;
GLuint gvPositionHandle;
GLuint gvColorHandle;
GLuint gvTexCoordinateHandle;
GLuint guTextureUniformId;
GLuint gvProjectionUniformId;
GLuint gvViewUniformId;
GLuint gvModelUniformId;
GLuint gTextureID;
osvr::clientkit::DisplayConfig* gOSVRDisplayConfig;
osvr::clientkit::ClientContext* gClientContext;
OSVR_ClientInterface gCamera = NULL;

GLuint createTexture(GLuint width, GLuint height) {
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

void updateTexture(GLuint width, GLuint height, GLubyte* data) {

    glBindTexture(GL_TEXTURE_2D, gTextureID);
    checkGlError("glBindTexture");

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // @todo use glTexSubImage2D to be faster here, but add check to make sure height/width are the same.
    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    //checkGlError("glTexSubImage2D");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    checkGlError("glTexImage2D");
}

void checkReturnCode(OSVR_ReturnCode returnCode, const char* msg) {
    if(returnCode != OSVR_RETURN_SUCCESS) {
        LOGI("[OSVR] OSVR method returned a failure: %s", msg);
        throw std::runtime_error(msg);
    }
}

OSVR_JointClientOpts createJointClientOpts()
{
    // @todo: dynamically get the file path, i.e. /data/data/com.osvr.android.gles2sample
    // might need to pass it in via JNI from the java activity - not sure if there is an NDK
    // API for it.
    OSVR_JointClientOpts opts = osvrJointClientCreateOptions();

    // This doesn't appear to work, even when setting LD_LIBRARY_PATH to
    // /data/data/com.osvr.android.gles2sample/files and copying the plugins
    // there. However, at least with this option I the init call succeeds
//    checkReturnCode(osvrJointClientOptionsAutoloadPlugins(opts),
//        "Auto-loading plugins...");

    // these two plugin calls result in the init call failing.
    // am I missing something?
//    checkReturnCode(osvrJointClientOptionsLoadPlugin(opts,
//        "com_osvr_Multiserver"),
//        "Loading multi-server");
//
    checkReturnCode(osvrJointClientOptionsLoadPlugin(opts,
        "com_osvr_android_sensorTracker"),
        "Loading android sensor plugin");

    checkReturnCode(osvrJointClientOptionsLoadPlugin(opts,
        "com_osvr_android_jniImaging"),
        "Loading android sensor plugin");
    // this causes the init call to fail
    // what is the path supposed to look like here?
    // Should this be "$.display" or "\"display\""?
    // Is it failing to read the LG_G4.json file?
    // @todo try to set the entire display json here as a hard-coded string blob
    checkReturnCode(osvrJointClientOptionsAddString(opts, "/display", "{\"meta\": { \"schemaVersion\": 1 }, \"hmd\": { \"device\": { \"vendor\": \"OSVR\", \"model\": \"HDK\", \"num_displays\": 1, \"Version\": \"1.1\", \"Note\": \"Settings are also good for HDK 1.0\" }, \"field_of_view\": { \"monocular_horizontal\": 90, \"monocular_vertical\": 101.25, \"overlap_percent\": 100, \"pitch_tilt\": 0 }, \"resolutions\": [ { \"width\": 2560, \"height\": 1440, \"video_inputs\": 1, \"display_mode\": \"horz_side_by_side\", \"swap_eyes\": 0 } ], \"distortion\": { \"k1_red\": 0, \"k1_green\": 0, \"k1_blue\": 0 }, \"rendering\": { \"right_roll\": 0, \"left_roll\": 0 }, \"eyes\": [ { \"center_proj_x\": 0.5, \"center_proj_y\": 0.5, \"rotate_180\": 0 }, { \"center_proj_x\": 0.5, \"center_proj_y\": 0.5, \"rotate_180\": 0 } ] } }"),
        "Setting display");

    // this seems to succeed when its the only action queued
    checkReturnCode(osvrJointClientOptionsTriggerHardwareDetect(opts),
        "Triggering hardware detect.");
    return opts;
}

static int gReportNumber = 0;
static OSVR_ImageBufferElement* gLastFrame = nullptr;
GLuint gLastFrameWidth = 0;
GLuint gLastFrameHeight = 0;
static GLubyte* gTextureBuffer = nullptr;
void imagingCallback(void *userdata, const OSVR_TimeValue *timestamp,
                     const OSVR_ImagingReport *report) {

    OSVR_ClientContext* ctx = (OSVR_ClientContext*)userdata;

    gReportNumber++;
    GLuint width = report->state.metadata.width;
    GLuint height = report->state.metadata.height;
    gLastFrameWidth = width;
    gLastFrameHeight = height;
    GLuint size = width * height * 4;

    gLastFrame = report->state.data;
}

bool setupOSVR() {
    try {
        // Is this necessary? Trying to make it easier for osvrJointClientKit
        // to find the plugins. This may not even be working as expected.
        boost::filesystem::current_path("/data/data/com.osvr.android.gles2sample/files");
        auto workingDirectory = boost::filesystem::current_path();
        LOGI("[OSVR] Current working directory: %s", workingDirectory.string().c_str());

        OSVR_JointClientOpts opts = createJointClientOpts();
        auto ctx = osvrJointClientInit("com.osvr.android.opengles2sample", opts);

        if (!ctx) {
            LOGI("[OSVR] Could not create the joint client/server context.");
            return false;
        }

        gClientContext = new osvr::clientkit::ClientContext(ctx);

        // temporary workaround to DisplayConfig issue,
        // display sometimes fails waiting for the tree from the server.
        for (int i = 0; i < 10000; i++) {
            gClientContext->update();
        }

        if(!gClientContext->checkStatus()) {
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

        if(!gOSVRDisplayConfig->checkStartup()) {
            LOGI("[OSVR] Timed out waiting for display to fully start up and receive the initial pose update.");
            return false;
        }

        LOGI("[OSVR] OK, display startup status is good!");
        OSVR_ClientContext contextC = gClientContext->get();
        if(OSVR_RETURN_SUCCESS != osvrClientGetInterface(contextC, "/camera", &gCamera)) {
            LOGI("Error, could not get the camera interface at /camera.");
            return false;
        }

        // Register the imaging callback.
        if(OSVR_RETURN_SUCCESS != osvrRegisterImagingCallback(gCamera, &imagingCallback, &contextC)) {
            LOGI("Error, could not register image callback.");
            return false;
        }

        return true;
    } catch(const std::runtime_error &ex) {
        LOGI("[OSVR] OSVR initialization failed: %s", ex.what());
        return false;
    }
}

bool setupGraphics(int width, int height) {
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

    gvTexCoordinateHandle = glGetAttribLocation(gProgram, "vTexCoordinate");
    checkGlError("glGetAttribLocation");
    LOGI("glGetAttribLocation(\"vTexCoordinate\") = %d\n", gvTexCoordinateHandle);

    gvProjectionUniformId = glGetUniformLocation(gProgram, "projection");
    gvViewUniformId = glGetUniformLocation(gProgram, "view");
    gvModelUniformId = glGetUniformLocation(gProgram, "model");
    guTextureUniformId = glGetUniformLocation(gProgram, "uTexture");

    glViewport(0, 0, width, height);
    checkGlError("glViewport");

    glDisable(GL_CULL_FACE);

    // @todo can we resize the texture after it has been created?
    // if not, we may have to delete the dummy one and create a new one after
    // the first imaging report.
    LOGI("Creating texture... here we go!");
    gTextureID = createTexture(width, height);
    return setupOSVR();
}

const GLfloat gTriangleColors[] = {
        0.75f, 0.0f, 0.0f, 0.75f,
        0.75f, 0.0f, 0.0f, 0.75f,
        1.0f, 0.0f, 0.0f, 1.0f,
        0.75f, 0.0f, 0.0f, 0.75f,
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,

        0.0f, 0.75f, 0.0f, 0.75f,
        0.0f, 0.75f, 0.0f, 0.75f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.75f, 0.0f, 0.75f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,

        0.0f, 0.0f, 0.75f, 0.75f,
        0.0f, 0.0f, 0.75f, 0.75f,
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.75f, 0.75f,
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,

        0.0f, 0.75f, 0.75f, 0.75f,
        0.0f, 0.75f, 0.75f, 0.75f,
        0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.75f, 0.75f, 0.75f,
        0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 1.0f, 1.0f,

        0.75f, 0.75f, 0.0f, 0.75f,
        0.75f, 0.75f, 0.0f, 0.75f,
        1.0f, 1.0f, 0.0f, 1.0f,
        0.75f, 0.75f, 0.0f, 0.75f,
        1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,

        0.75f, 0.0f, 0.75f, 0.75f,
        0.75f, 0.0f, 0.75f, 0.75f,
        1.0f, 0.0f, 1.0f, 1.0f,
        0.75f, 0.0f, 0.75f, 0.75f,
        1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f
};

const GLfloat gTriangleTexCoordinates[] = {
        // A cube face (letters are unique vertices)
        // A--B
        // |  |
        // D--C

        // As two triangles (clockwise)
        // A B D
        // B C D

        //glNormal3f(0.0, 0.0, -1.0);
        0.0f, 1.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        1.0f, 0.0f, // C
        0.0f, 0.0f, // D

        //glNormal3f(0.0, 0.0, 1.0);
        0.0f, 1.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        1.0f, 0.0f, // C
        0.0f, 0.0f, // D

//        glNormal3f(0.0, -1.0, 0.0);
        0.0f, 1.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        1.0f, 0.0f, // C
        0.0f, 0.0f, // D

//        glNormal3f(0.0, 1.0, 0.0);
        0.0f, 1.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        1.0f, 0.0f, // C
        0.0f, 0.0f, // D

//        glNormal3f(-1.0, 0.0, 0.0);
        0.0f, 1.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        1.0f, 0.0f, // C
        0.0f, 0.0f, // D

//        glNormal3f(1.0, 0.0, 0.0);
        0.0f, 1.0f, // A
        1.0f, 1.0f, // B
        0.0f, 0.0f, // D
        1.0f, 1.0f, // B
        1.0f, 0.0f, // C
        0.0f, 0.0f, // D
};

const GLfloat gTriangleVertices[] = {
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
void renderFrame() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    if(gOSVRDisplayConfig != NULL && gClientContext != NULL) {
        gClientContext->update();
        if(gLastFrame != nullptr) {
            updateTexture(gLastFrameWidth, gLastFrameHeight, gLastFrame);
            osvrClientFreeImage(gClientContext->get(), gLastFrame);
            gLastFrame = nullptr;
        }
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

                glVertexAttribPointer(gvTexCoordinateHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleTexCoordinates);
                checkGlError("glVertexAttribPointer");
                glEnableVertexAttribArray(gvTexCoordinateHandle);
                checkGlError("glEnableVertexAttribArray");

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, gTextureID);
                glUniform1i(guTextureUniformId, 0);

                glDrawArrays(GL_TRIANGLES, 0, 36);
                checkGlError("glDrawArrays");
            });
        });
    }
}

void stop() {
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

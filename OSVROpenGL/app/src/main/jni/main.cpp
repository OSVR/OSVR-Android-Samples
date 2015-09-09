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

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/ClientKit/InterfaceStateC.h>
#include <osvr/ClientKit/Display.h>

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
                                                    = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

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
                "varying vec4 fragmentColor;\n"
                "void main()\n"
                "{\n"
                "    gl_FragColor = fragmentColor;\n"
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
GLuint gvProjectionUniformId;
GLuint gvViewUniformId;
GLuint gvModelUniformId;
osvr::clientkit::DisplayConfig* gOSVRDisplayConfig;
osvr::clientkit::ClientContext* gClientContext;

bool setupGraphics(int w, int h) {
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    LOGI("setupGraphics(%d, %d)", w, h);
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

    glViewport(0, 0, w, h);
    checkGlError("glViewport");

    glDisable(GL_CULL_FACE);
    gClientContext = new osvr::clientkit::ClientContext(
            "com.osvr.exampleclients.TrackerState");

    // temporary workaround to DisplayConfig issue,
    // display sometimes fails waiting for the tree from the server.
    for(int i = 0; i < 10000; i++) {
        gClientContext->update();
    }

    gOSVRDisplayConfig = new osvr::clientkit::DisplayConfig(*gClientContext);
    if(!gOSVRDisplayConfig->valid()) {
        LOGI("[OSVR] Could not get a display config (server probably not "
            "running or not behaving), exiting");
        return false;
    }

    LOGI("[OSVR] Waiting for the display to fully start up, including "
        "receiving initial pose update...");
    while(!gOSVRDisplayConfig->checkStartup()) {
        gClientContext->update();
    }

    LOGI("[OSVR] OK, display startup status is good!");

    return true;
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

extern "C" {
    JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_init(JNIEnv * env, jobject obj,  jint width, jint height);
    JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_step(JNIEnv * env, jobject obj);
};

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_init(JNIEnv * env, jobject obj,  jint width, jint height)
{
    setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_step(JNIEnv * env, jobject obj)
{
    renderFrame();
}

//END_INCLUDE(all)

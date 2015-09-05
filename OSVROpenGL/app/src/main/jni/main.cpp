/*
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
        "attribute vec4 vPosition;\n"
                "uniform mat4 model;\n"
                "uniform mat4 view;\n"
                "uniform mat4 projection;\n"
                "void main() {\n"
                "  gl_Position = projection * view * model * vPosition;\n"
                "}\n";

static const char gFragmentShader[] =
        "precision mediump float;\n"
                "void main() {\n"
                "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
                "}\n";

// normally you'd load the shaders from a file, but in this case, let's
// just keep things simple and load from memory.
static const GLchar* vertexShader =
                "uniform mat4 model;\n"
                "uniform mat4 view;\n"
                "uniform mat4 projection;\n"
                "attribute vec4 position;\n"
                "attribute vec4 vertexColor;\n"
                "varying vec4 fragmentColor;\n"
                "void main()\n"
                "{\n"
                "   gl_Position = projection * view * inverse(model) * position;\n"
                "   fragmentColor = vertexColor;\n"
                "}\n";

static const GLchar* fragmentShader =
                "varying vec3 fragmentColor;\n"
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
GLuint gvProjectionUniformId;
GLuint gvViewUniformId;
GLuint gvModelUniformId;

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
    LOGI("glGetAttribLocation(\"vPosition\") = %d\n",
         gvPositionHandle);

    gvProjectionUniformId = glGetUniformLocation(gProgram, "projection");
    gvViewUniformId = glGetUniformLocation(gProgram, "view");
    gvModelUniformId = glGetUniformLocation(gProgram, "model");

    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    return true;
}

//class SampleShader {
//public:
//    SampleShader() { }
//
//    ~SampleShader() {
//        if (initialized) {
//            glDeleteProgram(programId);
//        }
//    }
//
//    void init() {
//        if (!initialized) {
//            LOGI("[OSVR] Initializing sample shader.");
////            GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
////            GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
////
////            // vertex shader
////            glShaderSource(vertexShaderId, 1, &vertexShader, NULL);
////            glCompileShader(vertexShaderId);
////            checkShaderError(vertexShaderId, "Vertex shader compilation failed.");
////
////            // fragment shader
////            glShaderSource(fragmentShaderId, 1, &fragmentShader, NULL);
////            glCompileShader(fragmentShaderId);
////            checkShaderError(fragmentShaderId, "Fragment shader compilation failed.");
////
////            // linking program
////            programId = glCreateProgram();
////            glAttachShader(programId, vertexShaderId);
////            glAttachShader(programId, fragmentShaderId);
////            glLinkProgram(programId);
////            checkProgramError(programId, "Shader program link failed.");
////
////            // once linked into a program, we no longer need the shaders.
////            glDeleteShader(vertexShaderId);
////            glDeleteShader(fragmentShaderId);
//
//            programId = createProgram(vertexShader, fragmentShader);
////            glBindAttribLocation(programId, 0, "position");
////            glBindAttribLocation(programId, 1, "vertexColor");
//
//            positionHandle = glGetAttribLocation(programId, "position");
//            colorHandle = glGetAttribLocation(programId, "vertexColor");
//
//            projectionUniformId = glGetUniformLocation(programId, "projection");
//            viewUniformId = glGetUniformLocation(programId, "view");
//            modelUniformId = glGetUniformLocation(programId, "model");
//            initialized = true;
//        }
//    }
//
//    void useProgram(const GLfloat projection[], const GLfloat view[], const GLfloat model[]) {
//        init();
//        LOGI("[OSVR] SampleShader::useProgram()");
//        glUseProgram(programId);
//
//        GLfloat projectionf[16];
//        GLfloat viewf[16];
//        GLfloat modelf[16];
//
//        convertMatrix(projection, projectionf);
//        convertMatrix(view, viewf);
//        convertMatrix(model, modelf);
//
//        glUniformMatrix4fv(projectionUniformId, 1, GL_FALSE, projectionf);
//        glUniformMatrix4fv(viewUniformId, 1, GL_FALSE, viewf);
//        glUniformMatrix4fv(modelUniformId, 1, GL_FALSE, modelf);
//    }
//
//    GLuint positionHandle;
//    GLuint colorHandle;
//private:
//    SampleShader(const SampleShader&) = delete;
//    SampleShader& operator=(const SampleShader&) = delete;
//    bool initialized = false;
//    GLuint programId = 0;
//    GLuint projectionUniformId = 0;
//    GLuint viewUniformId = 0;
//    GLuint modelUniformId = 0;
//
//    void checkShaderError(GLuint shaderId, const std::string& exceptionMsg) {
//        GLint result = GL_FALSE;
//        int infoLength = 0;
//        glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
//        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLength);
//        if (result == GL_FALSE) {
//            std::vector<GLchar> errorMessage(infoLength + 1);
//            glGetProgramInfoLog(programId, infoLength, NULL, &errorMessage[0]);
//            LOGI("[OSVR] program error: %s (%s), infoLength: %d", &errorMessage[0], exceptionMsg.c_str(), infoLength);
//            throw std::runtime_error(exceptionMsg);
//        }
//    }
//
//    void checkProgramError(GLuint programId, const std::string& exceptionMsg) {
//        GLint result = GL_FALSE;
//        int infoLength = 0;
//        glGetProgramiv(programId, GL_LINK_STATUS, &result);
//        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLength);
//        if (result == GL_FALSE) {
//            std::vector<GLchar> errorMessage(infoLength + 1);
//            glGetProgramInfoLog(programId, infoLength, NULL, &errorMessage[0]);
//            LOGI("[OSVR] program error: %s (%s), infoLength: %d", &errorMessage[0], exceptionMsg.c_str(), infoLength);
//            throw std::runtime_error(exceptionMsg);
//        }
//    }
//
//    void convertMatrix(const GLfloat source[], GLfloat dest_out[])
//    {
//        if (nullptr == source || nullptr == dest_out) {
//            throw new std::logic_error("source and dest_out must be non-null.");
//        }
//        for (int i = 0; i < 16; i++) {
//            dest_out[i] = source[i];
//        }
//    }
//};
//SampleShader sampleShader;
osvr::clientkit::DisplayConfig* osvrDisplayConfig;

const GLfloat gTriangleVertices[] = { 0.0f, 0.5f, -0.5f, -0.5f,
                                      0.5f, -0.5f };

//class Cube {
//public:
//    Cube(GLfloat scale) {
//        colorBufferData = {
//                0.0, 1.0, 0.0,
//                0.0, 1.0, 0.0,
//                0.0, 0.0, 1.0,
//                0.0, 0.0, 1.0,
//
//                0.0, 0.0, 1.0,
//                0.0, 0.0, 1.0,
//                1.0, 0.0, 0.0,
//                1.0, 0.0, 0.0,
//        };
//
//        vertexBufferData = {
//                -scale, -scale, scale,
//                scale, -scale, scale,
//                scale, scale, scale,
//                -scale, scale, scale,
//                -scale, -scale, -scale,
//                scale, -scale, -scale,
//                scale, scale, -scale,
//                -scale, scale, -scale,
//        };
//
//        vertexElementBufferData = {
//                0, 1, 2,
//                2, 3, 0,
//                3, 2, 6,
//                6, 7, 3,
//                7, 6, 5,
//                5, 4, 7,
//                4, 5, 1,
//                1, 0, 4,
//                4, 0, 3,
//                3, 7, 4,
//                1, 5, 6,
//                6, 2, 1,
//        };
//    }
//
//    ~Cube() {
//        if (initialized) {
//            glDeleteBuffers(1, &vertexBuffer);
//            glDeleteVertexArraysOES(1, &vertexArrayId);
//        }
//    }
//
//    void init() {
//        if (!initialized) {
////            LOGI("[OSVR] Initializing cube.");
////            // Vertex buffer
////            glGenBuffers(1, &vertexBuffer);
////            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
////            glBufferData(GL_ARRAY_BUFFER,
////                         sizeof(vertexBufferData[0]) * vertexBufferData.size(),
////                         &vertexBufferData[0],
////                         GL_STATIC_DRAW);
////            glBindBuffer(GL_ARRAY_BUFFER, 0);
////
////            // Vertex element buffer
////            glGenBuffers(1, &vertexElementBuffer);
////            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexElementBuffer);
////            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
////                         sizeof(vertexBufferData[0]) * vertexElementBufferData.size(),
////                         &vertexElementBufferData[0],
////                         GL_STATIC_DRAW);
////            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
////
////            // Color buffer
////            glGenBuffers(1, &colorBuffer);
////            glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
////            glBufferData(GL_ARRAY_BUFFER,
////                         sizeof(colorBufferData[0]) * colorBufferData.size(),
////                         &colorBufferData[0],
////                         GL_STATIC_DRAW);
////            glBindBuffer(GL_ARRAY_BUFFER, 0);
//
//            // Vertex array object
////            glGenVertexArraysOES(1, &vertexArrayId);
////            glBindVertexArrayOES(vertexArrayId); {
////                // color
////                glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
////                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
////
////                // VBO
////                glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
////                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
////
////                // EBO
////                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexElementBuffer);
////
////                glEnableVertexAttribArray(0);
////                glEnableVertexAttribArray(1);
////            } glBindVertexArrayOES(0);
//            initialized = true;
//        }
//    }
//
//    void draw(const GLfloat projection[], const GLfloat view[], const GLfloat model[]) {
//        init();
//        LOGI("[OSVR] Cube::draw()");
//        sampleShader.useProgram(projection, view, model);
//
//        glVertexAttribPointer(sampleShader.positionHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
//        checkGlError("glVertexAttribPointer");
//        glEnableVertexAttribArray(sampleShader.positionHandle);
//        checkGlError("glEnableVertexAttribArray");
//
//        glVertexAttribPointer(sampleShader.positionHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
//        checkGlError("glVertexAttribPointer");
//        glEnableVertexAttribArray(sampleShader.positionHandle);
//        checkGlError("glEnableVertexAttribArray");
//
//        glDrawArrays(GL_TRIANGLES, 0, 3);
//        checkGlError("glDrawArrays");
//
////        // color
////        glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
////        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
////
////        // VBO
////        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
////        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
////
////        // EBO
////        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexElementBuffer);
////
////        glEnableVertexAttribArray(0);
////        glEnableVertexAttribArray(1);
////
//////        glBindVertexArrayOES(vertexArrayId); {
////            glDrawElements(GL_TRIANGLES,
////                           static_cast<GLsizei>(vertexElementBufferData.size()),
////                           GL_UNSIGNED_INT, (GLvoid*)0);
////        } glBindVertexArrayOES(0);
//    }
//
//private:
//    Cube(const Cube&) = delete;
//    Cube& operator=(const Cube&) = delete;
//    bool initialized = false;
//    GLuint colorBuffer = 0;
//    GLuint vertexBuffer = 0;
//    GLuint vertexElementBuffer = 0;
//    GLuint vertexArrayId = 0;
//    std::vector<GLfloat> colorBufferData;
//    std::vector<GLfloat> vertexBufferData;
//    std::vector<GLuint> vertexElementBufferData;
//};
//
//Cube roomCube(1.0f);

/**
 * Just the current frame in the display.
 */
void renderFrame() {
    static float grey;
    grey += 0.01f;
    if (grey > 1.0f) {
        grey = 0.0f;
    }
    glClearColor(grey, grey, grey, 1.0f);
    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    glUseProgram(gProgram);
    checkGlError("glUseProgram");

    glVertexAttribPointer(gvPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
    checkGlError("glVertexAttribPointer");
    glEnableVertexAttribArray(gvPositionHandle);
    checkGlError("glEnableVertexAttribArray");
    glDrawArrays(GL_TRIANGLES, 0, 3);
    checkGlError("glDrawArrays");

//    glClearColor(0, 0, 0, 1.0f);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//    if(osvrDisplayConfig != NULL) {
//        LOGI("[OSVR] got osvrDisplayConfig. iterating through displays.");
//        osvrDisplayConfig->forEachEye([](osvr::clientkit::Eye eye) {
//
//            /// Try retrieving the view matrix (based on eye pose) from OSVR
//            GLfloat viewMat[OSVR_MATRIX_SIZE];
//            eye.getViewMatrix(OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS,
//                              viewMat);
//
//            /// For each display surface seen by the given eye of the given
//            /// viewer...
//            eye.forEachSurface([&viewMat](osvr::clientkit::Surface surface) {
//                auto viewport = surface.getRelativeViewport();
//                glViewport(static_cast<GLint>(viewport.left),
//                           static_cast<GLint>(viewport.bottom),
//                           static_cast<GLsizei>(viewport.width),
//                           static_cast<GLsizei>(viewport.height));
//
//                /// Set the OpenGL projection matrix based on the one we
//                /// computed.
//                GLfloat zNear = 0.1;
//                GLfloat zFar = 100;
//                GLfloat projMat[OSVR_MATRIX_SIZE];
//                surface.getProjectionMatrix(
//                        zNear, zFar, OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS |
//                                     OSVR_MATRIX_SIGNEDZ | OSVR_MATRIX_RHINPUT,
//                        projMat);
//
//                const static GLfloat identityMat4f[16] = {
//                        1.0f, 0.0f, 0.0f, 0.0f,
//                        0.0f, 1.0f, 0.0f, 0.0f,
//                        0.0f, 0.0f, 1.0f, 0.0f,
//                        0.0f, 0.0f, 0.0f, 1.0f,
//                };
//
//                /// Call out to render our scene.
//                renderScene(projMat, viewMat, identityMat4f);
//            });
//        });
//    }
}

///**
// * This is the main entry point of a native application that is using
// * android_native_app_glue.  It runs in its own thread, with its own
// * event loop for receiving input events and doing other things.
// */
//void android_main(struct android_app *state) {
//    struct engine engine;
//    osvr::clientkit::ClientContext context(
//            "com.osvr.exampleclients.TrackerState");
//
//    // temporary workaround to DisplayConfig issue,
//    // display sometimes fails waiting for the tree from the server.
//    for(int i = 0; i < 10000; i++) {
//        context.update();
//    }
//
//    osvr::clientkit::DisplayConfig display(context);
//    if(!display.valid()) {
//        LOGI("[OSVR] Could not get a display config (server probably not "
//            "running or not behaving), exiting");
//        return;
//    }
//
//    LOGI("[OSVR] Waiting for the display to fully start up, including "
//        "receiving initial pose update...");
//    while(!display.checkStartup()) {
//        context.update();
//    }
//
//    osvrDisplayConfig = &display;
//
//    LOGI("[OSVR] OK, display startup status is good!");
//
//    // loop waiting for stuff to do.
//    unsigned int i = 0;
//    std::stringstream ss;
//    while (1) {
//        context.update();
//
//        // Drawing is throttled to the screen update rate, so there
//        // is no need to do timing here.
//        renderFrame(&engine);
//    }
//}

extern "C" {
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv * env, jobject obj,  jint width, jint height);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv * env, jobject obj);
};

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv * env, jobject obj,  jint width, jint height)
{
    setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv * env, jobject obj)
{
    renderFrame();
}

//END_INCLUDE(all)

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
#include <errno.h>

#include <iostream>
#include <sstream>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/ClientKit/InterfaceStateC.h>
#include <osvr/ClientKit/Display.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))

PFNGLGENVERTEXARRAYSOESPROC glGenVertexArraysOES = (PFNGLGENVERTEXARRAYSOESPROC)eglGetProcAddress ( "glGenVertexArraysOES" );
PFNGLBINDVERTEXARRAYOESPROC glBindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC)eglGetProcAddress ( "glBindVertexArrayOES" );
PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSOESPROC)eglGetProcAddress ( "glDeleteVertexArraysOES" );

/**
 * Our saved state data.
 */
struct saved_state {
    float angle;
    int32_t x;
    int32_t y;
};

/**
 * Shared state for our app.
 */
struct engine {
    struct android_app* app;

//    ASensorManager* sensorManager;
//    const ASensor* accelerometerSensor;
//    ASensorEventQueue* sensorEventQueue;

    int animating;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    struct saved_state state;
    osvr::clientkit::DisplayConfig* disp;
};

/**
 * Initialize an EGL context for the current display.
 */
static int engine_init_display(struct engine* engine) {
    // initialize OpenGL ES and EGL

    /*
     * Here specify the attributes of the desired configuration.
     * Below, we select an EGLConfig with at least 8 bits per color
     * component compatible with on-screen windows
     */
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
    };
    EGLint w, h, dummy, format;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, 0, 0);

    /* Here, the application chooses the configuration it desires. In this
     * sample, we have a very simplified selection process, where we pick
     * the first EGLConfig that matches our criteria */
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
     * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
     * As soon as we picked a EGLConfig, we can safely reconfigure the
     * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    context = eglCreateContext(display, config, NULL, NULL);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    engine->display = display;
    engine->context = context;
    engine->surface = surface;
    engine->width = w;
    engine->height = h;
    engine->state.angle = 0;

    // Initialize GL state.
//    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glEnable(GL_CULL_FACE);
//    glShadeModel(GL_SMOOTH);
    glDisable(GL_DEPTH_TEST);

    return 0;
}

// normally you'd load the shaders from a file, but in this case, let's
// just keep things simple and load from memory.
static const GLchar* vertexShader =
        "#version 330 core\n"
                "layout(location = 0) in vec3 position;\n"
                "layout(location = 1) in vec3 vertexColor;\n"
                "out vec3 fragmentColor;\n"
                "uniform mat4 model;\n"
                "uniform mat4 view;\n"
                "uniform mat4 projection;\n"
                "void main()\n"
                "{\n"
                "   gl_Position = projection * view * inverse(model) * vec4(position,1);\n"
                "   fragmentColor = vertexColor;\n"
                "}\n";

static const GLchar* fragmentShader =
        "#version 330 core\n"
                "in vec3 fragmentColor;\n"
                "out vec3 color;\n"
                "void main()\n"
                "{\n"
                "    color = fragmentColor;\n"
                "}\n";

class SampleShader {
public:
    SampleShader() { }

    ~SampleShader() {
        if (initialized) {
            glDeleteProgram(programId);
        }
    }

    void init() {
        if (!initialized) {
            GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
            GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

            // vertex shader
            glShaderSource(vertexShaderId, 1, &vertexShader, NULL);
            glCompileShader(vertexShaderId);
            checkShaderError(vertexShaderId, "Vertex shader compilation failed.");

            // fragment shader
            glShaderSource(fragmentShaderId, 1, &fragmentShader, NULL);
            glCompileShader(fragmentShaderId);
            checkShaderError(fragmentShaderId, "Fragment shader compilation failed.");

            // linking program
            programId = glCreateProgram();
            glAttachShader(programId, vertexShaderId);
            glAttachShader(programId, fragmentShaderId);
            glLinkProgram(programId);
            checkProgramError(programId, "Shader program link failed.");

            // once linked into a program, we no longer need the shaders.
            glDeleteShader(vertexShaderId);
            glDeleteShader(fragmentShaderId);

            projectionUniformId = glGetUniformLocation(programId, "projection");
            viewUniformId = glGetUniformLocation(programId, "view");
            modelUniformId = glGetUniformLocation(programId, "model");
            initialized = true;
        }
    }

    void useProgram(const GLfloat projection[], const GLfloat view[], const GLfloat model[]) {
        init();
        glUseProgram(programId);
        GLfloat projectionf[16];
        GLfloat viewf[16];
        GLfloat modelf[16];
        convertMatrix(projection, projectionf);
        convertMatrix(view, viewf);
        convertMatrix(model, modelf);
        glUniformMatrix4fv(projectionUniformId, 1, GL_FALSE, projectionf);
        glUniformMatrix4fv(viewUniformId, 1, GL_FALSE, viewf);
        glUniformMatrix4fv(modelUniformId, 1, GL_FALSE, modelf);
    }

private:
    SampleShader(const SampleShader&) = delete;
    SampleShader& operator=(const SampleShader&) = delete;
    bool initialized = false;
    GLuint programId = 0;
    GLuint projectionUniformId = 0;
    GLuint viewUniformId = 0;
    GLuint modelUniformId = 0;

    void checkShaderError(GLuint shaderId, const std::string& exceptionMsg) {
        GLint result = GL_FALSE;
        int infoLength = 0;
        glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLength);
        if (result == GL_FALSE) {
            std::vector<GLchar> errorMessage(infoLength + 1);
            glGetProgramInfoLog(programId, infoLength, NULL, &errorMessage[0]);
            std::cerr << &errorMessage[0] << std::endl;
            throw std::runtime_error(exceptionMsg);
        }
    }

    void checkProgramError(GLuint programId, const std::string& exceptionMsg) {
        GLint result = GL_FALSE;
        int infoLength = 0;
        glGetProgramiv(programId, GL_LINK_STATUS, &result);
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLength);
        if (result == GL_FALSE) {
            std::vector<GLchar> errorMessage(infoLength + 1);
            glGetProgramInfoLog(programId, infoLength, NULL, &errorMessage[0]);
            std::cerr << &errorMessage[0] << std::endl;
            throw std::runtime_error(exceptionMsg);
        }
    }

    void convertMatrix(const GLfloat source[], GLfloat dest_out[])
    {
        if (nullptr == source || nullptr == dest_out) {
            throw new std::logic_error("source and dest_out must be non-null.");
        }
        for (int i = 0; i < 16; i++) {
            dest_out[i] = source[i];
        }
    }
};
static SampleShader sampleShader;

class Cube {
public:
    Cube(GLfloat scale) {
        colorBufferData = {
                0.0, 1.0, 0.0,
                0.0, 1.0, 0.0,
                0.0, 0.0, 1.0,
                0.0, 0.0, 1.0,

                0.0, 0.0, 1.0,
                0.0, 0.0, 1.0,
                1.0, 0.0, 0.0,
                1.0, 0.0, 0.0,
        };

        vertexBufferData = {
                -scale, -scale, scale,
                scale, -scale, scale,
                scale, scale, scale,
                -scale, scale, scale,
                -scale, -scale, -scale,
                scale, -scale, -scale,
                scale, scale, -scale,
                -scale, scale, -scale,
        };

        vertexElementBufferData = {
                0, 1, 2,
                2, 3, 0,
                3, 2, 6,
                6, 7, 3,
                7, 6, 5,
                5, 4, 7,
                4, 5, 1,
                1, 0, 4,
                4, 0, 3,
                3, 7, 4,
                1, 5, 6,
                6, 2, 1,
        };
    }

    ~Cube() {
        if (initialized) {
            glDeleteBuffers(1, &vertexBuffer);
            glDeleteVertexArraysOES(1, &vertexArrayId);
        }
    }

    void init() {
        if (!initialized) {
            // Vertex buffer
            glGenBuffers(1, &vertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(vertexBufferData[0]) * vertexBufferData.size(),
                         &vertexBufferData[0],
                         GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Vertex element buffer
            glGenBuffers(1, &vertexElementBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexElementBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         sizeof(vertexBufferData[0]) * vertexElementBufferData.size(),
                         &vertexElementBufferData[0],
                         GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            // Color buffer
            glGenBuffers(1, &colorBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(colorBufferData[0]) * colorBufferData.size(),
                         &colorBufferData[0],
                         GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Vertex array object
            glGenVertexArraysOES(1, &vertexArrayId);
            glBindVertexArrayOES(vertexArrayId); {
                // color
                glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

                // VBO
                glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

                // EBO
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexElementBuffer);

                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);
            } glBindVertexArrayOES(0);
            initialized = true;
        }
    }

    void draw(const GLfloat projection[], const GLfloat view[], const GLfloat model[]) {
        init();

        sampleShader.useProgram(projection, view, model);

        glBindVertexArrayOES(vertexArrayId); {
            glDrawElements(GL_TRIANGLES,
                           static_cast<GLsizei>(vertexElementBufferData.size()),
                           GL_UNSIGNED_INT, (GLvoid*)0);
        } glBindVertexArrayOES(0);
    }

private:
    Cube(const Cube&) = delete;
    Cube& operator=(const Cube&) = delete;
    bool initialized = false;
    GLuint colorBuffer = 0;
    GLuint vertexBuffer = 0;
    GLuint vertexElementBuffer = 0;
    GLuint vertexArrayId = 0;
    std::vector<GLfloat> colorBufferData;
    std::vector<GLfloat> vertexBufferData;
    std::vector<GLuint> vertexElementBufferData;
};

static Cube roomCube(1.0f);


/// @brief A simple dummy "draw" function - note that drawing occurs in "room
/// space" by default. (that is, in this example, the modelview matrix when this
/// function is called is initialized such that it transforms from world space
/// to view space)
static void renderScene(const GLfloat projection[], const GLfloat view[], const GLfloat model[]) {
    roomCube.draw(projection, view, model);
}

/**
 * Just the current frame in the display.
 */
static void engine_draw_frame(struct engine* engine) {
    if (engine->display == NULL) {
        // No display.
        return;
    }

    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(engine->disp != NULL) {
        engine->disp->forEachEye([](osvr::clientkit::Eye eye) {

            /// Try retrieving the view matrix (based on eye pose) from OSVR
            GLfloat viewMat[OSVR_MATRIX_SIZE];
            eye.getViewMatrix(OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS,
                              viewMat);
            /// Initialize the ModelView transform with the view matrix we
            /// received
            //glMatrixMode(GL_MODELVIEW);
            //glLoadIdentity();
            //glMultMatrixd(viewMat);

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
                GLfloat zNear = 0.1;
                GLfloat zFar = 100;
                GLfloat projMat[OSVR_MATRIX_SIZE];
                surface.getProjectionMatrix(
                        zNear, zFar, OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS |
                                     OSVR_MATRIX_SIGNEDZ | OSVR_MATRIX_RHINPUT,
                        projMat);

//                glMatrixMode(GL_PROJECTION);
//                glLoadIdentity();
//                glMultMatrixd(projMat);

//                /// Set the matrix mode to ModelView, so render code doesn't
//                /// mess with the projection matrix on accident.
//                glMatrixMode(GL_MODELVIEW);

                const static GLfloat identityMat4f[16] = {
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f,
                };

                /// Call out to render our scene.
                renderScene(projMat, viewMat, identityMat4f);
            });
        });
    }

    eglSwapBuffers(engine->display, engine->surface);
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void engine_term_display(struct engine* engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->animating = 0;
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

/**
 * Process the next input event.
 */
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* engine = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        engine->animating = 1;
        engine->state.x = AMotionEvent_getX(event, 0);
        engine->state.y = AMotionEvent_getY(event, 0);
        return 1;
    }
    return 0;
}

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* engine = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            // The system has asked us to save our current state.  Do so.
            engine->app->savedState = malloc(sizeof(struct saved_state));
            *((struct saved_state*)engine->app->savedState) = engine->state;
            engine->app->savedStateSize = sizeof(struct saved_state);
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (engine->app->window != NULL) {
                engine_init_display(engine);
                engine_draw_frame(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            engine_term_display(engine);
            break;
        case APP_CMD_GAINED_FOCUS:
            // When our app gains focus, we start monitoring the accelerometer.
//            if (engine->accelerometerSensor != NULL) {
//                ASensorEventQueue_enableSensor(engine->sensorEventQueue,
//                        engine->accelerometerSensor);
//                // We'd like to get 60 events per second (in us).
//                ASensorEventQueue_setEventRate(engine->sensorEventQueue,
//                        engine->accelerometerSensor, (1000L/60)*1000);
//            }
            break;
        case APP_CMD_LOST_FOCUS:
            // When our app loses focus, we stop monitoring the accelerometer.
            // This is to avoid consuming battery while not being used.
//            if (engine->accelerometerSensor != NULL) {
//                ASensorEventQueue_disableSensor(engine->sensorEventQueue,
//                        engine->accelerometerSensor);
//            }
            // Also stop animating.
            engine->animating = 0;
            engine_draw_frame(engine);
            break;
    }
}

extern "C" {
    /**
     * This is the main entry point of a native application that is using
     * android_native_app_glue.  It runs in its own thread, with its own
     * event loop for receiving input events and doing other things.
     */
    void android_main(struct android_app *state) {
        struct engine engine;
        osvr::clientkit::ClientContext context(
                "com.osvr.exampleclients.TrackerState");

        osvr::clientkit::DisplayConfig display(context);
        if(!display.valid()) {
            LOGI("[OSVR] Could not get a display config (server probably not "
                "running or not behaving), exiting");
            return;
        }

        LOGI("[OSVR] Waiting for the display to fully start up, including "
            "receiving initial pose update...");
        while(!display.checkStartup()) {
            context.update();
        }

        engine.disp = &display;

        LOGI("[OSVR] OK, display startup status is good!");

        // Make sure glue isn't stripped.
        app_dummy();

        memset(&engine, 0, sizeof(engine));
        state->userData = &engine;
        state->onAppCmd = engine_handle_cmd;
        state->onInputEvent = engine_handle_input;
        engine.app = state;
        engine.animating = true;

        // Prepare to monitor accelerometer
//        engine.sensorManager = ASensorManager_getInstance();
//        engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,
//                                                                     ASENSOR_TYPE_ACCELEROMETER);
//        engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,
//                                                                  state->looper, LOOPER_ID_USER, NULL,
//                                                                  NULL);

        if (state->savedState != NULL) {
            // We are starting with a previous saved state; restore from it.
            engine.state = *(struct saved_state *) state->savedState;
        }

        // loop waiting for stuff to do.
        unsigned int i = 0;
        std::stringstream ss;
        while (1) {
            context.update();

            // Read all pending events.
            int ident;
            int events;
            struct android_poll_source *source;

            // If not animating, we will block forever waiting for events.
            // If animating, we loop until all events are read, then continue
            // to draw the next frame of animation.
            while ((ident = ALooper_pollAll(engine.animating ? 0 : -1, NULL, &events,
                                            (void **) &source)) >= 0) {

                // Process this event.
                if (source != NULL) {
                    source->process(state, source);
                }

                // If a sensor has data, process it now.
//                if (ident == LOOPER_ID_USER) {
//                    if (engine.accelerometerSensor != NULL) {
//                        ASensorEvent event;
//                        while (ASensorEventQueue_getEvents(engine.sensorEventQueue,
//                                                           &event, 1) > 0) {
//                            LOGI("accelerometer: x=%f y=%f z=%f",
//                                 event.acceleration.x, event.acceleration.y,
//                                 event.acceleration.z);
//                        }
//                    }
//                }

                // Check if we are exiting.
                if (state->destroyRequested != 0) {
                    engine_term_display(&engine);
                    return;
                }
            }

            if (engine.animating) {
                // Done with events; draw next animation frame.
                engine.state.angle += .01f;
                if (engine.state.angle > 1) {
                    engine.state.angle = 0;
                }

                // Drawing is throttled to the screen update rate, so there
                // is no need to do timing here.
                engine_draw_frame(&engine);
            }
        }
    }
}
//END_INCLUDE(all)

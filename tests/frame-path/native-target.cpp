#include "native-target.hpp"
#include <EGL/egl.h>
#include <GL/glx.h>
QSize nativeTargetSize()
{
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
        EGLint width = 0, height = 0;
        eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_WIDTH, &width);
        eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_HEIGHT, &height);
        return QSize(width, height);
    }
    unsigned width = 0, height = 0;
    glXQueryDrawable(glXGetCurrentDisplay(), glXGetCurrentDrawable(), GLX_WIDTH, &width);
    glXQueryDrawable(glXGetCurrentDisplay(), glXGetCurrentDrawable(), GLX_HEIGHT, &height);
    return QSize(width, height);
}

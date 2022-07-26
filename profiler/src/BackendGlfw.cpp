#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_opengl3_loader.h"

#include <chrono>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#ifdef _WIN32
#  define GLFW_EXPOSE_NATIVE_WIN32
#  include <GLFW/glfw3native.h>
#elif defined __linux__
#  ifdef DISPLAY_SERVER_X11
#    define GLFW_EXPOSE_NATIVE_X11
#  elif defined DISPLAY_SERVER_WAYLAND
#    define GLFW_EXPOSE_NATIVE_WAYLAND
#  else
#    error "unsupported linux display server"
#  endif
#  include <GLFW/glfw3native.h>
#endif

#include "Backend.hpp"
#include "RunQueue.hpp"


static GLFWwindow* s_window;
static std::function<void()> s_redraw;
static RunQueue* s_mainThreadTasks;

static void glfw_error_callback( int error, const char* description )
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}


Backend::Backend( const char* title, std::function<void()> redraw, RunQueue* mainThreadTasks )
{
    glfwSetErrorCallback( glfw_error_callback );
    if( !glfwInit() ) exit( 1 );
#ifdef DISPLAY_SERVER_WAYLAND
    glfwWindowHint( GLFW_ALPHA_BITS, 0 );
#else
    glfwWindowHint(GLFW_VISIBLE, 0 );
#endif
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 2 );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
#if __APPLE__
    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
#endif
    s_window = glfwCreateWindow( m_winPos.w, m_winPos.h, title, NULL, NULL );
    if( !s_window ) exit( 1 );

    glfwSetWindowPos( s_window, m_winPos.x, m_winPos.y );
#ifdef GLFW_MAXIMIZED
    if( m_winPos.maximize ) glfwMaximizeWindow( s_window );
#endif

    glfwMakeContextCurrent( s_window );
    glfwSwapInterval( 1 ); // Enable vsync
    glfwSetWindowRefreshCallback( s_window, []( GLFWwindow* ) { s_redraw(); } );

    ImGui_ImplGlfw_InitForOpenGL( s_window, true );
    ImGui_ImplOpenGL3_Init( "#version 150" );

    s_redraw = redraw;
    s_mainThreadTasks = mainThreadTasks;
}

Backend::~Backend()
{
#ifdef GLFW_MAXIMIZED
    uint32_t maximized = glfwGetWindowAttrib( s_window, GLFW_MAXIMIZED );
    if( maximized ) glfwRestoreWindow( s_window );
#else
    uint32_t maximized = 0;
#endif
    m_winPos.maximize = maximized;

    glfwGetWindowPos( s_window, &m_winPos.x, &m_winPos.y );
    glfwGetWindowSize( s_window, &m_winPos.w, &m_winPos.h );

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    glfwDestroyWindow( s_window );

    glfwTerminate();
}

void Backend::Show()
{
    glfwShowWindow( s_window );
}

void Backend::Run()
{
    while( !glfwWindowShouldClose( s_window ) )
    {
        glfwPollEvents();
        if( glfwGetWindowAttrib( s_window, GLFW_ICONIFIED ) )
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
            continue;
        }
        s_redraw();
        if( !glfwGetWindowAttrib( s_window, GLFW_FOCUSED ) )
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        }
        s_mainThreadTasks->Run();
    }
}

void Backend::NewFrame( int& w, int& h )
{
    glfwGetFramebufferSize( s_window, &w, &h );
    m_w = w;
    m_h = h;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void Backend::EndFrame()
{
    const ImVec4 clear_color = ImColor( 114, 144, 154 );

    ImGui::Render();
    glViewport( 0, 0, m_w, m_h );
    glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
    glClear( GL_COLOR_BUFFER_BIT );
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
    if( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent( backup_current_context );
    }

    glfwSwapBuffers( s_window );
}

void Backend::SetIcon( uint8_t* data, int w, int h )
{
    GLFWimage icon;
    icon.width = w;
    icon.height = h;
    icon.pixels = data;
    glfwSetWindowIcon( s_window, 1, &icon );
}

void Backend::SetTitle( const char* title )
{
    glfwSetWindowTitle( s_window, title );
}

float Backend::GetDpiScale()
{
#if GLFW_VERSION_MAJOR > 3 || ( GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3 )
    auto monitor = glfwGetWindowMonitor( s_window );
    if( !monitor ) monitor = glfwGetPrimaryMonitor();
    if( monitor )
    {
        float x, y;
        glfwGetMonitorContentScale( monitor, &x, &y );
        return x;
    }
#endif
    return 1;
}

void* Backend::GetNativeWindow()
{
#ifdef _WIN32
    return (void*)glfwGetWin32Window( s_window );
#elif defined __linux__
#  ifdef DISPLAY_SERVER_X11
    return (void*)glfwGetX11Window( s_window );
#  elif defined DISPLAY_SERVER_WAYLAND
    return (void*)glfwGetWaylandWindow( s_window );
#  endif
#else
    return nullptr;
#endif
}

#ifndef Magnum_ImGuiIntegration_Context_h
#define Magnum_ImGuiIntegration_Context_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 ShaddyAQN <ShaddyAQN@gmail.com>
    Copyright © 2018 Tomáš Skřivan <skrivantomas@seznam.cz>
    Copyright © 2018 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2019 bowling-allie <allie.smith.epic@gmail.com>
    Copyright © 2022, 2024 Pablo Escobar <mail@rvrs.in>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
 * @brief Class @ref Magnum::ImGuiIntegration::Context
 */

#include <Corrade/Containers/String.h>
#include <Magnum/Timeline.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Shaders/FlatGL.h>

#include "Magnum/ImGuiIntegration/visibility.h"

#include <list>

#ifndef DOXYGEN_GENERATING_OUTPUT
struct ImGuiContext;
struct ImTextureData;
#endif

namespace Magnum { namespace ImGuiIntegration {

namespace Implementation {
    template<class Application, class = void> struct ApplicationClipboard;
}

/**
@brief Dear ImGui context

Handles initialization and destruction of a Dear ImGui context and implements a
Magnum-based rendering backend.

@section ImGuiIntegration-Context-usage Usage

Creating the @ref Context instance will create the Dear ImGui context and make
it current. From that point on you can use ImGui calls.

@snippet ImGuiIntegration.cpp Context-usage

After setting up the context you can call @ref connectApplicationClipboard() if
you want ImGui to access the clipboard. If your application implementation
doesn't support clipboard access, ImGui's default (local) clipboard
implementation will be used.

@subsection ImGuiIntegration-Context-usage-rendering Rendering

Use @ref newFrame() to initialize a ImGui frame and finally draw it with
@ref drawFrame() to the currently bound framebuffer. Dear ImGui requires
@ref GL::Renderer::Feature::ScissorTest "scissor test" to be enabled and
@ref GL::Renderer::Feature::DepthTest "depth test" to be disabled.
@ref GL::Renderer::Feature::Blending "Blending" should be enabled and set up as
below. The following snippet sets up all required renderer state and then
resets it back to default values. Adapt the state changes based on what else
you are rendering. Right before @ref drawFrame() you can call
@ref updateApplicationCursor() if your application implementation supports
setting cursors.

@snippet ImGuiIntegration-sdl2.cpp Context-usage-per-frame

<b></b>

@m_class{m-note m-warning}

@par
    As shown above, because Dear ImGui has frame-based event handling, you're
    * *required* to constantly @ref Platform::Sdl2Application::redraw() "redraw()",
    instead of just waiting on input events. While that's not a problem for
    games, for regular apps that unfortunately means your application will use
    the CPU even when completely idle.

<b></b>

@m_class{m-block m-info}

@par Reduced renderer state setup
    The above assumes you're drawing ImGui together with something else (a 3D
    scene in the background, for example). If you only draw ImGui alone, it's
    enough (and also faster) to set just the following in the constructor,
    without doing any renderer state changes each frame:
@par
    @snippet ImGuiIntegration.cpp Context-usage-state-imgui-only

@subsection ImGuiIntegration-Context-usage-events Event handling

The templated @ref handleMousePressEvent(), @ref handleMouseReleaseEvent() etc.
functions are meant to be used inside event handlers of application classes
such as @ref Platform::Sdl2Application, directly passing the @p event parameter
to them. The returned value is then @cpp true @ce if ImGui used the event (and
thus it shouldn't be propagated further) and @cpp false @ce otherwise.

@snippet ImGuiIntegration-sdl2.cpp Context-events

<b></b>

@m_class{m-note m-warning}

@par
    As shown above, implementation of those templated functions is provided in
    a separate @ref ImGuiIntegration/Context.hpp file --- don't forget to
    include it at the point of use to avoid linker errors. This is done in
    order to optimize compile times, as the event handling implementation is
    rather large. See @ref compilation-speedup-hpp for more information.

@subsection ImGuiIntegration-Context-usage-text-input Text input

UTF-8 text input is handled via @ref handleTextInputEvent() but the application
implementations only call
@ref Platform::Sdl2Application::textInputEvent() "textInputEvent()" when text
input is enabled. This is done because some platforms require explicit action
in order to start a text input (for example, to open an on-screen keyboard on
touch devices, or [IME](https://en.wikipedia.org/wiki/Input_method) for complex
alphabets). ImGui exposes its desire to capture text input during a call to
@ref newFrame(). Based on that, you can toggle the text input in the
application, for example using
@ref Platform::Sdl2Application::startTextInput() "startTextInput()" /
@ref Platform::Sdl2Application::stopTextInput() "stopTextInput()" in
@ref Platform::Sdl2Application or @link Platform::GlfwApplication @endlink:

@snippet ImGuiIntegration-sdl2.cpp Context-text-input

The above snippet also means that ImGui's @cpp InputQueueCharacters @ce will be
empty unless an text input box is focused --- so if you want to handle text
input through ImGui manually, you need to explicitly call
@ref Platform::Sdl2Application::startTextInput() "startTextInput()" /
@ref Platform::Sdl2Application::stopTextInput() "stopTextInput()" when desired.

@section ImGuiIntegration-Context-fonts Loading custom fonts

The @ref Context class does additional adjustments to ImGui font setup in order
to make their scaling DPI-aware. If you load custom fonts, it's recommended to
do that before the @ref Context class is created, in which case it picks up the
custom font as default. Create the ImGui context first, add the font and then
construct the integration using the @ref Context(ImGuiContext&, const Vector2i&)
constructor, passing the already created ImGui context to it:

@snippet ImGuiIntegration.cpp Context-custom-fonts

It's possible to load custom fonts after the @ref Context instance been
constructed as well, but you first need to clear the default font added during
@ref Context construction and finally call @ref relayout() to make it pick up
the updated glyph cache. Alternatively, if you don't call @cpp Clear() @ce, you
need to explicitly call @cpp PushFont() @ce to switch to a non-default one.
Compared to loading fonts before the @ref Context is created, this is the less
efficient option, as the glyph cache is unnecessarily built and discarded one
more time.

@snippet ImGuiIntegration-sdl2.cpp Context-custom-fonts-after

See the @ref ImGuiIntegration-Context-dpi "DPI awareness" section below for
more information about configuring the fonts for HiDPI screens.

@m_class{m-block m-warning}

@par Loading fonts from memory
    Note that, when using @cpp AddFontFromMemoryTTF() @ce (for example
    to load a font from @ref Corrade::Utility::Resource), ImGui by default
    takes over the memory ownership. In order to avoid memory corruption on
    exit, you need to explicitly tell it to *not* do that by setting
    @cpp ImFontConfig::FontDataOwnedByAtlas @ce to @cpp false @ce:
@par
    @snippet ImGuiIntegration.cpp Context-custom-fonts-resource

@section ImGuiIntegration-Context-dpi DPI awareness

There are three separate concepts for DPI-aware UI rendering:

-   UI size --- size of the user interface to which all widgets are positioned
-   Window size --- size of the window to which all input events are related
-   Framebuffer size --- size of the framebuffer the UI is being rendered to

Depending on the platform and use case, each of these three values can be
different. For example, a game menu screen can have the UI size the same
regardless of window size. Or on Retina macOS you can have different window and
framebuffer size and the UI size might be related to window size but
independent on the framebuffer size.

When using for example @ref Platform::Sdl2Application or other `*Application`
implementations, you usually have three values at your disposal ---
@ref Platform::Sdl2Application::windowSize() "windowSize()",
@ref Platform::Sdl2Application::framebufferSize() "framebufferSize()" and
@ref Platform::Sdl2Application::dpiScaling() "dpiScaling()". ImGui interfaces
are usually positioned with pixel units, getting more room on bigger windows.
A non-DPI-aware setup would be simply this:

@snippet ImGuiIntegration-sdl2.cpp Context-dpi-unaware

If you want the UI to keep a reasonable physical size and stay crisp with
different pixel densities, pass a ratio of window size and DPI scaling to the
UI size:

@snippet ImGuiIntegration-sdl2.cpp Context-dpi-aware

Finally, by clamping the first @p size parameter you can achieve various other
results like limiting it to a minimal / maximal area or have it fully scaled
with window size. When window size, framebuffer size or DPI scaling changes
(usually as a response to @ref Platform::Sdl2Application::viewportEvent() "viewportEvent()"),
call @ref relayout() with the new values. If the pixel density is changed, this
will result in the font caches being rebuilt.

@m_class{m-note m-warning}

@par
    Additional steps are needed on some platforms in order to make the
    executable itself DPI-aware --- otherwise it will appear blurry on HiDPI
    displays. See the corresponding sections in
    @ref platforms-windows-hidpi "Windows" and
    @ref platforms-macos-hidpi "macOS / iOS" platform docs for details.

@subsection ImGuiIntegration-Context-dpi-fonts HiDPI fonts

@note
    The default font used by ImGui, [Proggy Clean](https://www.dafont.com/proggy-clean.font),
    is a bitmap one, becoming rather blurry and blocky in larger sizes. It's
    recommended to switch to a different font for a crisper experience on HiDPI
    screens.

There are further important steps for DPI awareness if you are supplying custom
fonts. Use the @ref Context(ImGuiContext&, const Vector2&, const Vector2i&, const Vector2i&)
constructor and pre-scale their size by the ratio of @p size and
@p framebufferSize. If you don't do that, the fonts will appear tiny on HiDPI
screens. Example:

@snippet ImGuiIntegration-sdl2.cpp Context-custom-fonts-dpi

If you supplied custom fonts and pixel density changed, in order to regenerate
them you have to clear the font atlas and re-add all fonts again with a
different scaling *before* calling @ref relayout(), for example:

@snippet ImGuiIntegration-sdl2.cpp Context-relayout-fonts-dpi

If you don't do that, the fonts stay at the original scale, not matching the
new UI scaling anymore. If you didn't supply any custom font, the function will
reconfigure the builtin font automatically.

@section ImGuiIntegration-Context-large-meshes Large meshes

Complex user interfaces or widgets like [ImPlot](https://github.com/epezent/implot)
may end up creating large meshes with more than 65k vertices. Because ImGui
defaults to 16-bit index buffers this can lead to asserts or visual errors.

If the underlying GL context supports @ref GL::Mesh::setBaseVertex() "setting the base vertex for indexed meshes"
the rendering backend sets the @cpp ImGuiBackendFlags_RendererHasVtxOffset @ce
flag. This lets ImGui know the backend can handle per-draw vertex offsets,
removing the 65k limitation altogether. Support for that requires one of the
following:

@requires_gl32 Extension @gl_extension{ARB,draw_elements_base_vertex}
@requires_gles32 Extension @gl_extension{OES,draw_elements_base_vertex} or
    @gl_extension{EXT,draw_elements_base_vertex}
@requires_webgl_extension WebGL 2.0 and extension
    @webgl_extension{WEBGL,draw_instanced_base_vertex_base_instance}

If you can't guarantee that the required GL versions or extensions will be
available at runtime (especially relevant on WebGL), the next best option is to
change ImGui's index type to 32-bit by adding the following line to the
@ref ImGuiIntegration-configuration "ImGui user config":

@code{.cpp}
#define ImDrawIdx unsigned int
@endcode

This doubles the size of the index buffer, resulting in potentially reduced
draw performance, but is guaranteed to work on all GL versions.

@section ImGuiIntegration-Context-custom-textures Drawing custom textures

In order to draw a @ref GL::Texture2D instance, use the
@ref image() and @ref imageButton() utilities in
@ref Magnum/ImGuiIntegration/Widgets.h. For low-level texture drawing with
ImGui APIs that accept a `ImTextureID`, use the @ref textureId() helper to
create an ImGui texture ID from a @ref GL::Texture2D reference.

@section ImGuiIntegration-Context-multiple-contexts Multiple contexts

Each instance of @ref Context creates a new ImGui context. You can also pass an
existing context to the @ref Context(ImGuiContext&, const Vector2i&)
constructor, which will then take ownership (and thus delete it on
destruction). Switching between various @cpp ImGui @ce contexts wrapped in
@ref Context instances is done automatically when calling any of the
@ref relayout(), @ref newFrame(), @ref drawFrame() APIs or the event handling
functions. You can also query the instance-specific context with @ref context()
and call @cpp ImGui::SetCurrentContext() @ce manually on that.

It's also possible to create a context-less instance using the
@ref Context(NoCreateT) constructor and release context ownership using
@ref release(). Such instances, together with moved-out instances are empty and
calling any API that interacts with ImGui is not allowed on these.

@experimental
*/
class MAGNUM_IMGUIINTEGRATION_EXPORT Context {
    public:
        /**
         * @brief Constructor
         * @param size                  Size of the user interface to which all
         *      widgets are positioned
         * @param windowSize            Size of the window to which all input
         *      events are related
         * @param framebufferSize       Size of the window framebuffer. On
         *      some platforms with HiDPI screens may be different from window
         *      size.
         *
         * This function creates the ImGui context using
         * @cpp ImGui::CreateContext() @ce and then queries the font glyph
         * cache from ImGui, uploading it to the GPU. If you need to do some
         * extra work on the context and before the font texture gets uploaded,
         * use @ref Context(ImGuiContext&, const Vector2&, const Vector2i&, const Vector2i&)
         * instead.
         *
         * The sizes are allowed to be zero in any dimension, but note that
         * specifying a concrete value later in @ref relayout() may trigger an
         * unnecessary rebuild of the font glyph cache due to different
         * calculated pixel density. See @ref ImGuiIntegration-Context-dpi for
         * more information about the different size arguments. If you don't
         * need DPI awareness, you can use the simpler
         * @ref Context(const Vector2i&) constructor instead.
         * @see @ref relayout(const Vector2&, const Vector2i&, const Vector2i&)
         */
        explicit Context(const Vector2& size, const Vector2i& windowSize, const Vector2i& framebufferSize);

        /**
         * @brief Construct without DPI awareness
         *
         * Equivalent to calling @ref Context(const Vector2&, const Vector2i&, const Vector2i&)
         * with @p size passed to all three parameters.
         * @see @ref relayout(const Vector2i&)
         */
        explicit Context(const Vector2i& size);

        /**
         * @brief Construct from an existing context
         * @param context               Existing ImGui context
         * @param size                  Size of the user interface to which all
         *      widgets are positioned
         * @param windowSize            Size of the window to which all inputs
         *      events are related
         * @param framebufferSize       Size of the window framebuffer. On
         *      some platforms with HiDPI screens may be different from window
         *      size.
         *
         * Expects that no instance is created yet; takes ownership of the
         * passed context, deleting it on destruction. In comparison to
         * @ref Context(const Vector2&, const Vector2i&, const Vector2i&) this
         * constructor is useful if you need to do some work before the font
         * glyph cache gets uploaded to the GPU, for example adding custom
         * fonts.
         *
         * See @ref ImGuiIntegration-Context-dpi for more information about the
         * different size arguments. If you don't need DPI awareness, you can
         * use the simpler @ref Context(ImGuiContext&, const Vector2i&)
         * constructor instead. Note that, in order to have the custom fonts
         * crisp also on HiDPI screens, you have to pre-scale their size by the
         * ratio of @p size and @p framebufferSize. See
         * @ref ImGuiIntegration-Context-dpi-fonts for more information.
         * @see @ref relayout(const Vector2&, const Vector2i&, const Vector2i&)
         */
        explicit Context(ImGuiContext& context, const Vector2& size, const Vector2i& windowSize, const Vector2i& framebufferSize);

        /**
         * @brief Construct from an existing context without DPI awareness
         *
         * Equivalent to calling @ref Context(ImGuiContext&, const Vector2&, const Vector2i&, const Vector2i&)
         * with @p size passed to the last three parameters. In comparison to
         * @ref Context(const Vector2i&) this constructor is useful if you need
         * to do some work before the font glyph cache gets uploaded to the
         * GPU, for example @ref ImGuiIntegration-Context-fonts "adding custom fonts".
         * @see @ref relayout(const Vector2i&)
         */
        explicit Context(ImGuiContext& context, const Vector2i& size);

        /**
         * @brief Construct without creating the underlying ImGui context
         *
         * This constructor also doesn't create any internal OpenGL objects,
         * meaning it can be used without an active OpenGL context. Calling any
         * APIs that interact with ImGui on such instance is not allowed. Move
         * a non-empty instance over to make it useful.
         * @see @ref context(), @ref release()
         */
        explicit Context(NoCreateT) noexcept;

        /** @brief Copying is not allowed */
        Context(const Context&) = delete;

        /** @brief Move constructor */
        Context(Context&& other) noexcept;

        /**
         * @brief Destructor
         *
         * If @ref context() is not @cpp nullptr @ce, makes it current using
         * @cpp ImGui::SetCurrentContext() @ce and then calls
         * @cpp ImGui::DestroyContext() @ce.
         */
        ~Context();

        /** @brief Copying is not allowed */
        Context& operator=(const Context&) = delete;

        /** @brief Move assignment */
        Context& operator=(Context&& other) noexcept;

        /**
         * @brief Underlying ImGui context
         *
         * Returns @cpp nullptr @ce if this is a @ref NoCreate "NoCreate"d,
         * moved-out or @ref release() "release()"d instance.
         */
        ImGuiContext* context() { return _context; }

        /**
         * @brief Release the underlying ImGui context
         *
         * Returns the underlying ImGui context and sets the internal context
         * pointer to @cpp nullptr @ce, making the instance equivalent to a
         * moved-out state. Calling APIs that interact with ImGui is not
         * allowed on the instance anymore.
         */
        ImGuiContext* release();

        /**
         * @brief Font texture used in `ImFontAtlas`
         * @m_since_{integration,2020,06}
         */
        GL::Texture2D& atlasTexture() { return _texture; }

        /**
         * @brief Relayout the context
         * @param size                  Size of the user interface to which all
         *      widgets are positioned
         * @param windowSize            Size of the window to which all input
         *      events are related
         * @param framebufferSize       Size of the window framebuffer. On
         *      some platforms with HiDPI screens may be different from window
         *      size.
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() and
         * adapts the internal state for a new window size or pixel density. In
         * case the pixel density gets changed, font glyph caches are rebuilt
         * to match the new pixel density.
         *
         * The sizes are allowed to be zero in any dimension, but note that it
         * may trigger an unwanted rebuild of the font glyph cache due to
         * different calculated pixel density. See
         * @ref ImGuiIntegration-Context-dpi for more information about the
         * different size arguments. If you don't need DPI awareness, you can
         * use the simpler @ref relayout(const Vector2i&) function instead.
         */
        void relayout(const Vector2& size, const Vector2i& windowSize, const Vector2i& framebufferSize);

        /**
         * @brief Relayout the context
         *
         * Equivalent to calling @ref relayout(const Vector2&, const Vector2i&, const Vector2i&)
         * with @p size passed to all three parameters.
         */
        void relayout(const Vector2i& size);

        /**
         * @brief Start a new frame
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() and
         * initializes a new ImGui frame using @cpp ImGui::NewFrame() @ce. This
         * function also decides if a text input needs to be enabled, see
         * @ref ImGuiIntegration-Context-usage-text-input for more information.
         */
        void newFrame();

        /**
         * @brief Draw a frame
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context(),
         * @cpp ImGui::Render() @ce and then draws the frame created by ImGui
         * calls since last call to @ref newFrame() to currently bound
         * framebuffer.
         *
         * See @ref ImGuiIntegration-Context-usage-rendering for more
         * information on which rendering states to set before and after
         * calling this method.
         */
        void drawFrame();

        /**
         * @brief Handle pointer press event
         * @m_since_latest_{integration}
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::pointerPressEvent(), to ImGui.
         * Returns @cpp true @ce if ImGui wants to capture the mouse (so the
         * event shouldn't be further propagated to the rest of the
         * application), @cpp false @ce otherwise.
         *
         * If the event isn't primary (such as a second and following finger
         * press in a multi-touch scenario), the function does nothing and
         * returns @cpp false @ce.
         */
        template<class PointerEvent> bool handlePointerPressEvent(PointerEvent& event);

        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Handle mouse press event
         * @m_deprecated_since_latest Use @ref handlePointerPressEvent() with a
         *      corresponding @relativeref{Platform::Sdl2Application,PointerEvent}
         *      instance instead.
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::mousePressEvent(), to ImGui. Returns
         * @cpp true @ce if ImGui wants to capture the mouse (so the event
         * shouldn't be further propagated to the rest of the application),
         * @cpp false @ce otherwise.
         */
        template<class MouseEvent> CORRADE_DEPRECATED("use handlePointerPressEvent() with a corresponding PointerEvent instance instead") bool handleMousePressEvent(MouseEvent& event);
        #endif

        /**
         * @brief Handle pointer release event
         * @m_since_latest_{integration}
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::pointerReleaseEvent(), to ImGui.
         * Returns @cpp true @ce if ImGui wants to capture the mouse (so the
         * event shouldn't be further propagated to the rest of the
         * application), @cpp false @ce otherwise.
         *
         * If the event isn't primary (such as a second and following finger
         * press in a multi-touch scenario), the function does nothing and
         * returns @cpp false @ce.
         */
        template<class PointerEvent> bool handlePointerReleaseEvent(PointerEvent& event);

        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Handle mouse release event
         * @m_deprecated_since_latest Use @ref handlePointerReleaseEvent() with
         *      a corresponding
         *      @relativeref{Platform::Sdl2Application,PointerEvent} instance
         *      instead.
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::mouseReleaseEvent(), to ImGui.
         * Returns @cpp true @ce if ImGui wants to capture the mouse (so the
         * event shouldn't be further propagated to the rest of the
         * application), @cpp false @ce otherwise.
         */
        template<class MouseEvent> CORRADE_DEPRECATED("use handlePointerReleaseEvent() with a corresponding PointerEvent instance instead") bool handleMouseReleaseEvent(MouseEvent& event);
        #endif

        /**
         * @brief Handle scroll event
         * @m_since_latest_{integration}
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::scrollEvent(), to ImGui. Returns
         * @cpp true @ce if ImGui wants to capture the mouse (so the event
         * shouldn't be further propagated to the rest of the application),
         * @cpp false @ce otherwise.
         */
        template<class ScrollEvent> bool handleScrollEvent(ScrollEvent& event);

        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Handle mouse scroll event
         * @m_deprecated_since_latest Use @ref handleScrollEvent() with a
         *      corresponding @relativeref{Platform::Sdl2Application,ScrollEvent}
         *      instance instead.
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::mouseScrollEvent(), to ImGui.
         * Returns @cpp true @ce if ImGui wants to capture the mouse (so the
         * event shouldn't be further propagated to the rest of the
         * application), @cpp false @ce otherwise.
         */
        template<class MouseScrollEvent> CORRADE_DEPRECATED("use handleScrollEvent() with a corresponding ScrollEvent instance instead") bool handleMouseScrollEvent(MouseScrollEvent& event);
        #endif

        /**
         * @brief Handle pointer move event
         * @m_since_latest_{integration}
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::pointerMoveEvent(), to ImGui.
         * Returns @cpp true @ce if ImGui wants to capture the mouse (so the
         * event shouldn't be further propagated to the rest of the
         * application), @cpp false @ce otherwise.
         *
         * If the event isn't primary (such as a second and following finger
         * press in a multi-touch scenario), the function does nothing and
         * returns @cpp false @ce.
         */
        template<class PointerMoveEvent> bool handlePointerMoveEvent(PointerMoveEvent& event);

        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Handle mouse move event
         * @m_deprecated_since_latest Use @ref handlePointerMoveEvent() with a
         *      corresponding
         *      @relativeref{Platform::Sdl2Application,PointerMoveEvent}
         *      instance instead.
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::mouseMoveEvent(), to ImGui. Returns
         * @cpp true @ce if ImGui wants to capture the mouse (so the event
         * shouldn't be further propagated to the rest of the application),
         * @cpp false @ce otherwise.
         */
        template<class MouseMoveEvent> CORRADE_DEPRECATED("use handlePointerMoveEvent() with a corresponding PointerMoveEvent instance instead") bool handleMouseMoveEvent(MouseMoveEvent& event);
        #endif

        /**
         * @brief Handle key press event
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::keyPressEvent(), to ImGui. Returns
         * @cpp true @ce if ImGui wants to capture the keyboard (so the event
         * shouldn't be further propagated to the rest of the application),
         * @cpp false @ce otherwise.
         */
        template<class KeyEvent> bool handleKeyPressEvent(KeyEvent& event);

        /**
         * @brief Handle key release event
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::keyReleaseEvent(), to ImGui. Returns
         * @cpp true @ce if ImGui wants to capture the keyboard (so the event
         * shouldn't be further propagated to the rest of the application),
         * @cpp false @ce otherwise.
         */
        template<class KeyEvent> bool handleKeyReleaseEvent(KeyEvent& event);

        /**
         * @brief Handle text input event
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then propagates the event, such as the one coming from
         * @ref Platform::Sdl2Application::textInputEvent(), to ImGui. Returns
         * @cpp true @ce if ImGui wants to capture the keyboard (so the event
         * shouldn't be further propagated to the rest of the application),
         * @cpp false @ce otherwise.
         */
        template<class TextInputEvent> bool handleTextInputEvent(TextInputEvent& event);

        /**
         * @brief Update application mouse cursor
         * @m_since_{integration,2020,06}
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then queries @cpp ImGui::GetMouseCursor() @ce, propagating that
         * to the application via @ref Platform::Sdl2Application::setCursor() "setCursor()".
         * If the application doesn't implement a corresponding cursor, falls
         * back to @ref Platform::Sdl2Application::Cursor::Arrow "Cursor::Arrow".
         */
        template<class Application> void updateApplicationCursor(Application& application);

        /**
         * @brief Connect application clipboard
         * @m_since_latest_{integration}
         *
         * Calls @cpp ImGui::SetCurrentContext() @ce on @ref context() first
         * and then sets up the clipboard callbacks, connecting them with the
         * application via @relativeref{Platform::Sdl2Application,clipboardText()}
         * and @relativeref{Platform::Sdl2Application,setClipboardText()}. If
         * the application doesn't implement a clipboard, does nothing.
         */
        template<class Application> void connectApplicationClipboard(Application& application);

    private:
        void updateTexture(ImTextureData* tex);

        template<class Application, class> friend struct Implementation::ApplicationClipboard;

        ImGuiContext* _context;
        Shaders::FlatGL2D _shader;
        GL::Texture2D _texture{NoCreate};
        GL::Buffer _vertexBuffer{GL::Buffer::TargetHint::Array};
        GL::Buffer _indexBuffer{GL::Buffer::TargetHint::ElementArray};
        Timeline _timeline;
        GL::Mesh _mesh;
        Vector2 _supersamplingRatio,
            _eventScaling;
        /* Optionally used by connectApplicationClipboard() */
        void* _application;
        Containers::String _lastClipboardText;

        std::list<GL::Texture2D> _textures;

    private:
        template<class KeyEvent> bool handleKeyEvent(KeyEvent& event, bool value);
        template<class PointerEvent> bool handlePointerEvent(PointerEvent& event, bool value);
        #ifdef MAGNUM_BUILD_DEPRECATED
        template<class MouseEvent> bool handleMouseEvent(MouseEvent& event, bool value);
        #endif
};

}}

#endif

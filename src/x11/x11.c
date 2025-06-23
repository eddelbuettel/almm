#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __USE_XOPEN     // for strptime
#include <time.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>

#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>

#include <hiredis/hiredis.h>
#include <errno.h>

#include "../cairo_draw_text.h"
#include "../log.h"
#include "../options.h"

// Structure to hold parsed stock data
typedef struct {
    char fmttime[32];
    double time;
    double open;
    double high;
    double low;
    double close;
    long volume;
    double percent_change;
    double change;
    char symbol[16];
    int updated;
    char title[64];
    char subtitle[64];
} stock_data_t;

// Global variables for Redis
stock_data_t current_stock_data = {0};
time_t most_recent = 0;
redisContext *redis_ctx = NULL;

// generated function: returns XEvent name
const char *XEventName(int type);
// check if compositor is running
static bool compositor_check(Display *d, int screen)
{
    char prop_name[16];
    snprintf(prop_name, 16, "_NET_WM_CM_S%d", screen);
    Atom prop_atom = XInternAtom(d, prop_name, False);
    return XGetSelectionOwner(d, prop_atom) != None;
}

// -- beginning of added Redis support
// Function to parse semicolon-separated stock data string
int parse_stock_data(const char* data_str, stock_data_t* stock_data) {
    if (!data_str || !stock_data) {
        return -1;
    }

    // Create a copy of the string for strtok
    char* data_copy = strdup(data_str);
    if (!data_copy) {
        return -1;
    }

    char* token;
    int field_count = 0;

    // Parse each field separated by semicolon, see the repo
    // https://github.com/eddelbuettel/redis-pubsub-example
    // for a sample producer (and a simpler consumer)
    token = strtok(data_copy, ";");
    while (token && field_count < 8) {
        switch (field_count) {
            case 0: // formatted time
                memcpy(stock_data->fmttime, token, strlen(token));
                break;
            case 1: // open
                stock_data->open = atof(token);
                break;
            case 2: // high
                stock_data->high = atof(token);
                break;
            case 3: // low
                stock_data->low = atof(token);
                break;
            case 4: // close
                stock_data->close = atof(token);
                break;
            case 5: // volume
                stock_data->volume = atol(token);
                break;
            case 6: // percent_change
                stock_data->percent_change = atof(token);
                break;
            case 7: // change
                stock_data->change = atof(token);
                break;
        }
        field_count++;
        token = strtok(NULL, ";");
    }

    free(data_copy);

    struct tm tm;
    strptime(stock_data->fmttime, "%Y-%m-%d %H:%M:%S", &tm);
    stock_data->time = mktime(&tm);
    if (stock_data->time > most_recent) {
        stock_data->updated = 1;
        most_recent = stock_data->time;
        __info__("Seeing updated data for %s at %s\n", stock_data->symbol, stock_data->fmttime);
    } else {
        stock_data->updated = 0;
    }
    
    __info__("Field count %d\n", field_count);
    return (field_count == 8) ? 0 : -1;
}

void assign_rgb_colors(double chg, float cols[9][3]) {
    int p = chg / 0.125;        // truncating division used on purpose here
    p = (p > 8) ? 8 : p; 	// so that we don't need fmin() and hence -lm linking */
    options.text_color = rgba_color_new(cols[p][0], cols[p][1], cols[p][2], 0.6);
}

void set_rgb_colors(double chg) {
    // these are from ColorBrewer and are the red and green four valued multi-hue
    // values, divided by 255 to fit the [0, 1) range here
    // see
    //   RColorBrewer::display.brewer.pal(9, "Reds")
    // use
    //   M <- col2rgb(RColorBrewer::brewer.pal(9, "Reds"))/255
    //   for (i in 1:9) cat("{ ", paste(sprintf("%.8f",M[,i]), collapse=", "), "},\n")
    float reds[9][3] = {
        {  1.00000000, 0.96078431, 0.94117647 },
        {  0.99607843, 0.87843137, 0.82352941 },
        {  0.98823529, 0.73333333, 0.63137255 },
        {  0.98823529, 0.57254902, 0.44705882 },
        {  0.98431373, 0.41568627, 0.29019608 },
        {  0.93725490, 0.23137255, 0.17254902 },
        {  0.79607843, 0.09411765, 0.11372549 },
        {  0.64705882, 0.05882353, 0.08235294 },
        {  0.40392157, 0.00000000, 0.05098039 }
    };
    // see
    //   RColorBrewer::display.brewer.pal(9, "Greens")
    // use
    //   M <- col2rgb(RColorBrewer::brewer.pal(9, "Greens"))/255
    //   for (i in 1:9) cat("{ ", paste(sprintf("%.8f",M[,i]), collapse=", "), "},\n")
    float greens[9][3] = {
        {  0.96862745, 0.98823529, 0.96078431 },
        {  0.89803922, 0.96078431, 0.87843137 },
        {  0.78039216, 0.91372549, 0.75294118 },
        {  0.63137255, 0.85098039, 0.60784314 },
        {  0.45490196, 0.76862745, 0.46274510 },
        {  0.25490196, 0.67058824, 0.36470588 },
        {  0.13725490, 0.54509804, 0.27058824 },
        {  0.00000000, 0.42745098, 0.17254902 },
        {  0.00000000, 0.26666667, 0.10588235 }
    };
    if (chg < 0)
        assign_rgb_colors(-chg, reds);
    else
        assign_rgb_colors(chg, greens);
}

// Function to format stock data and time into activate-linux fields
void draw_stock_data() {
    sprintf(current_stock_data.title, "%.2f %+.2f %+.3f%%",
            current_stock_data.close,
            current_stock_data.change,
            current_stock_data.percent_change);
    sprintf(current_stock_data.subtitle, "%s @ %s",
            current_stock_data.symbol,
            current_stock_data.fmttime);
    options.title = current_stock_data.title;
    options.subtitle = current_stock_data.subtitle;
    set_rgb_colors(current_stock_data.percent_change);
}

// Function to handle Redis pub/sub messages
int handle_redis_messages() {
    redisReply *reply;

    // Use redisGetReply to read the next reply
    if (redisGetReply(redis_ctx, (void**)&reply) != REDIS_OK) {
        if (redis_ctx->err) {
            printf("Redis error: %s\n", redis_ctx->errstr);
            return -1;
        }
        return 0; // No data available
    }

    if (!reply) {
        return 0;
    }

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 3) {
        char* message_type = reply->element[0]->str;
        char* channel = reply->element[1]->str;
        char* message = reply->element[2]->str;

        __info__("Redis message - Type: %s, Channel: %s, Data: %s\n", message_type, channel, message);

        if (strcmp(message_type, "message") == 0) {
            // Parse and update stock data
            strncpy(current_stock_data.symbol, channel,
                    sizeof(current_stock_data.symbol) - 1);
            current_stock_data.symbol[sizeof(current_stock_data.symbol) - 1] = '\0';

            if (parse_stock_data(message, &current_stock_data) == 0) {
                if (current_stock_data.updated == 1) {
                    __info__("Stock data updated for %s to %s\n", channel, message);
                    draw_stock_data();
                } else {
                    __info__("Ignoring message %s:%s\n", channel, message);
                }
            } else {
                printf("Error parsing stock data: %s\n", message);
            }
        } else if (strcmp(message_type, "subscribe") == 0) {
            printf("Successfully subscribed to channel: %s\n", channel);
        }
    } else {
        printf("Unexpected reply type: %d\n", reply->type);
    }

    freeReplyObject(reply);
    return 1;
}

// -- end of added Redis support

int x11_backend_start(void)
{
    // init_redis_subscription START
    const char* symbols[] = { "SP500", "ES1" };
    const char* host = "127.0.0.1";
    int port = 6379;
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

    redis_ctx = redisConnectWithTimeout(host, port, timeout);
    if (!redis_ctx || redis_ctx->err) {
        if (redis_ctx) {
            printf("Error: %s\n", redis_ctx->errstr);
            redisFree(redis_ctx);
        } else {
            printf("Error: Can't allocate redis context\n");
        }
        return -1;
    }

    __info__("Connected to Redis server %s:%d\n", host, port);

    for (size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
        const char* symbol = symbols[i];

        // Subscribe to the symbol
        redisReply *reply = redisCommand(redis_ctx, "SUBSCRIBE %s", symbol);
        if (!reply) {
            printf("Error: Failed to subscribe to %s\n", symbol);
            return -1;
        }

        __info__("Subscribed to symbol: %s\n", symbol);

        // Handle subscription confirmation
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 3) {
            __debug__("Subscription confirmed for: %s\n", reply->element[1]->str);
        }

        freeReplyObject(reply);
    }
    // init_redis_subscription END

    __debug__("Opening display\n");
    Display *d = XOpenDisplay(NULL);
    __debug__("Finding root window\n");
    Window root = DefaultRootWindow(d);
    __debug__("Finding default screen\n");
    int default_screen = XDefaultScreen(d);

    __debug__("Checking compositor\n");
    bool compositor_running = compositor_check(d, XDefaultScreen(d));
    if (!compositor_running)
    {
        __info__("No running compositor detected. Program may not work as intended\n");
    }
	if (options.force_xshape == true) {
		__debug__("Forcing XShape\n");
		compositor_running = false;
	}
    // https://x.org/releases/current/doc/man/man3/Xinerama.3.xhtml
    __debug__("Finding all screens in use using Xinerama\n");
    int num_entries = 0, min_entry = 1;
    XineramaScreenInfo *si = XineramaQueryScreens(d, &num_entries);
    // if xinerama fails
    if (si == NULL)
    {
        __perror__(
            "Required X extension Xinerama is not active. It is needed for displaying watermark on multiple screens");
        XCloseDisplay(d);
        return 1;
    }
    __debug__("Found %d screen(s)\n", num_entries);

    // https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
    __debug__("Initializing Xrandr\n");
    int xrr_error_base;
    int xrr_event_base;
    if (!XRRQueryExtension(d, &xrr_event_base, &xrr_error_base))
    {
        __perror__("Required X extension Xrandr is not active. It is needed for handling screen size change (e.g. in "
                   "virtual machine window)");
        XFree(si);
        XCloseDisplay(d);
        return 1;
    }
    __debug__("Subscribing on screen change events\n");
    XRRSelectInput(d, root, RRScreenChangeNotifyMask);

    XSetWindowAttributes attrs;
    attrs.override_redirect = 1;

    XVisualInfo vinfo;

    // MacOS doesn't support 32 bit color through XQuartz, massive hack
#ifdef __APPLE__
    int colorDepth = 24;
#else
    int colorDepth = 32;
#endif

    __debug__("Checking default screen to be %d bit color depth\n", colorDepth);
    if (!XMatchVisualInfo(d, default_screen, colorDepth, TrueColor, &vinfo))
    {
        __error__("No screens supporting %i bit color found, terminating\n", colorDepth);
        exit(EXIT_FAILURE);
    }

    __debug__("Set %d bit color depth\n", colorDepth);
    attrs.colormap = XCreateColormap(d, root, vinfo.visual, AllocNone);
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;

    Window overlay[num_entries];
    cairo_surface_t *surface[num_entries];
    cairo_t *cairo_ctx[num_entries];

    Pixmap xshape_mask[num_entries];
    cairo_surface_t *xshape_surface[num_entries];
    cairo_t *xshape_ctx[num_entries];

    int overlay_height = options.overlay_height * options.scale;
    __debug__("Scaled height: %d px\n", overlay_height);
    int overlay_width = options.overlay_width * options.scale;
    __debug__("Scaled width:  %d px\n", overlay_width);

    for (int i = min_entry; i < num_entries; i++)
    {
        __debug__("Creating overlay on %d screen\n", i);
        overlay[i] = XCreateWindow(d,                                                                // display
                                   root,                                                             // parent
                                   si[i].x_org + si[i].width - overlay_width,  // x position
                                   si[i].y_org + si[i].height - overlay_height, // y position
                                   overlay_width,                                                    // width
                                   overlay_height,                                                   // height
                                   0,                                                                // border width
                                   vinfo.depth,                                                      // depth
                                   InputOutput,                                                      // class
                                   vinfo.visual,                                                     // visual
                                   CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel,    // value mask
                                   &attrs                                                            // attributes
        );
        // subscribe to Exposure Events, required for redrawing after DPMS blanking
        XSelectInput(d, overlay[i], ExposureMask);
        XMapWindow(d, overlay[i]);

        // allows the mouse to click through the overlay
        XRectangle rect;
        XserverRegion region = XFixesCreateRegion(d, &rect, 1);
        XFixesSetWindowShapeRegion(d, overlay[i], ShapeInput, 0, 0, region);
        XFixesDestroyRegion(d, region);

        // sets a WM_CLASS to allow the user to blacklist some effect from compositor
        XClassHint *xch = XAllocClassHint();
        xch->res_name = "activate-linux";
        xch->res_class = "activate-linux";
        XSetClassHint(d, overlay[i], xch);

        // set _NET_WM_BYPASS_COMPOSITOR
        // https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html#idm45446104333040
        if (options.bypass_compositor)
        {
            __debug__("Bypassing compositor\n");
            unsigned char data = 1;
            XChangeProperty(d, overlay[i], XInternAtom(d, "_NET_WM_BYPASS_COMPOSITOR", False), XA_CARDINAL, 32,
                            PropModeReplace, &data, 1);
        }

        if (options.gamescope_overlay)
        {
            // https://github.com/Plagman/gamescope/issues/288
            // https://github.com/flightlessmango/MangoHud/blob/9a6809daca63cf6860ac9d92ae4b2dde36239b0e/src/app/main.cpp#L47
            // https://github.com/flightlessmango/MangoHud/blob/9a6809daca63cf6860ac9d92ae4b2dde36239b0e/src/app/main.cpp#L189
            // https://github.com/trigg/Discover/blob/de83063f3452b1cdee89b4c3779103eae2c90cbb/discover_overlay/overlay.py#L107

            __debug__("Setting GAMESCOPE_EXTERNAL_OVERLAY\n");
            unsigned char data = 1;
            XChangeProperty(d, overlay[i], XInternAtom(d, "GAMESCOPE_EXTERNAL_OVERLAY", False), XA_CARDINAL, 32,
                            PropModeReplace, &data, 1);
        }

        __debug__("Creating cairo context\n");
        surface[i] = cairo_xlib_surface_create(d, overlay[i], vinfo.visual, overlay_width, overlay_height);
        cairo_ctx[i] = cairo_create(surface[i]);

        __debug__("Creating cario surface/context and mask pixmap for XShape support\n");
        if (!compositor_running)
        {
            xshape_mask[i] = XCreatePixmap(d, overlay[i], overlay_width, overlay_height, 1);
            xshape_surface[i] = cairo_xlib_surface_create_for_bitmap(d, xshape_mask[i], DefaultScreenOfDisplay(d),
                                                                     overlay_width, overlay_height);
            xshape_ctx[i] = cairo_create(xshape_surface[i]);
        }
    }

    __info__("All done. Going into X windows event endless loop\n\n");
    XEvent event;
    // init_redis_subscription START
    fd_set read_fds;
    int x11_fd = ConnectionNumber(d);
    int redis_fd = redis_ctx->fd;
    int max_fd = (x11_fd > redis_fd) ? x11_fd : redis_fd;
    //struct timeval timeout;
    // init_redis_subscription END

    while (1)
    {
        // Prepare file descriptor set
        FD_ZERO(&read_fds);
        FD_SET(x11_fd, &read_fds);
        FD_SET(redis_fd, &read_fds);

        // Set timeout for select (100ms)
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        __debug__("Before select in endless loop\n");
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        __debug__("After select in endless loop\n");

        if (ready < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, retry
            }
            perror("select");
            break;
        }

        // Handle Redis messages
        if (ready > 0 && FD_ISSET(redis_fd, &read_fds)) {
            // Process all available Redis messages
            //while (handle_redis_messages() > 0) {
            handle_redis_messages();
                // Keep processing until no more messages
            //}
            __info__("Text now set, num_entries %d\n", num_entries);
            for (int i = min_entry; i < num_entries; i++) {
                draw_text(cairo_ctx[i], 0);
            }
        }

        // Check for Redis connection errors
        if (redis_ctx->err) {
            printf("Redis connection error: %s\n", redis_ctx->errstr);
            break;
        }



        while (XPending(d)) {
            __info__("Before XNextEvent in endless loop\n");
            XNextEvent(d, &event);
            __info__("After XNextEvent in endless loop\n");
            // handle screen resize via catching Xrandr event
            if (XRRUpdateConfiguration(&event))
                {
                    if (event.type - xrr_event_base == RRScreenChangeNotify)
                        {
                            __debug__("! Got Xrandr event about screen change\n");
                            __debug__("  Updating info about screen sizes\n");
                            si = XineramaQueryScreens(d, &num_entries);
                            for (int i = 0; i < num_entries; i++)
                                {
                                    __debug__("  Moving window on screen %d according new position\n", i);
                                    XMoveWindow(d,                                                               // display
                                                overlay[i],                                                      // window
                                                si[i].x_org + si[i].width - overlay_width, // x position
                                                si[i].y_org + si[i].height - overlay_height // y position
                                        );
                                }
                        }
                    else
                        {
                            __debug__("! Got Xrandr event, type: %d (0x%X)\n", event.type - xrr_event_base,
                                      event.type - xrr_event_base);
                        }
                }
            else if (event.type == Expose)
                {
                    /*
                     * See https://www.x.org/releases/X11R7.5/doc/man/man3/XExposeEvent.3.html
                     * removed draw_text() call from elsewhere because XExposeEvent is emitted
                     * on both window init and window damage.
                     */

                    __debug__("! Got X event, type: %s (0x%X)\n", XEventName(event.type), event.type);
                    for (int i = 0; i < num_entries && event.xexpose.count == 0; i++)
                        {
                            if (overlay[i] == event.xexpose.window)
                                {
                                    __debug__("  Redrawing overlay: %d\n", i);

                                    if (!compositor_running)
                                        {
                                            __debug__("Shaping window %d using XShape\n", i);
                                            draw_text(cairo_ctx[i], 2);
                                            draw_text(xshape_ctx[i], 1);
                                            XShapeCombineMask(d, overlay[i], ShapeBounding, 0, 0,
                                                              cairo_xlib_surface_get_drawable(xshape_surface[i]), ShapeSet);
                                        } else {
                                        draw_text(cairo_ctx[i], 0);
                                    }
                                    break;
                                }
                        }
                }
            else
                {
                    __debug__("! Got X event, type: %s (0x%X)\n", XEventName(event.type), event.type);
                }
        }
    }

    // free used resources
    for (int i = 0; i < num_entries; i++)
    {
        XUnmapWindow(d, overlay[i]);
        cairo_destroy(cairo_ctx[i]);
        cairo_surface_destroy(surface[i]);
    }

    if (!compositor_running)
    {
  	  for (int i = 0; i < num_entries; i++)
   	  {
    	    XFreePixmap(d, xshape_mask[i]);
            cairo_destroy(xshape_ctx[i]);
            cairo_surface_destroy(xshape_surface[i]);
	  }
    }

    XFree(si);
    XCloseDisplay(d);

    return 0;
}

int x11_backend_kill_running(void)
{
    __error__("x11_backend_kill_running currently is not implemented\n");
    return 1;
}

#include "bc_utils.h"

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#define M_PI           3.14159265358979323846 

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_D3D11_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_d3d11.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Shell32.lib")

#include <shellapi.h>
PBYTE viewerContent;
DWORD viewerLimit = 4096;

static IDXGISwapChain* swap_chain;
static ID3D11Device* device;
static ID3D11DeviceContext* context;
static ID3D11RenderTargetView* rt_view;

static void
set_swap_chain_size(int width, int height)
{
    ID3D11Texture2D* back_buffer;
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    HRESULT hr;

    if (rt_view)
        ID3D11RenderTargetView_Release(rt_view);

    ID3D11DeviceContext_OMSetRenderTargets(context, 0, NULL, NULL);

    hr = IDXGISwapChain_ResizeBuffers(swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
    {
        /* to recover from this, you'll need to recreate device and all the resources */
        MessageBoxW(NULL, L"DXGI device is removed or reset!", L"Error", 0);
        exit(0);
    }
    assert(SUCCEEDED(hr));

    memset(&desc, 0, sizeof(desc));
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    hr = IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void**)&back_buffer);
    assert(SUCCEEDED(hr));

    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)back_buffer, &desc, &rt_view);
    assert(SUCCEEDED(hr));

    ID3D11Texture2D_Release(back_buffer);
}

static LRESULT CALLBACK
WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (swap_chain)
        {
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            set_swap_chain_size(width, height);
            nk_d3d11_resize(context, width, height);
        }
        break;
    }

    if (nk_d3d11_handle_event(wnd, msg, wparam, lparam))
        return 0;

    return DefWindowProcW(wnd, msg, wparam, lparam);
}

//static void bufferAppend(PBYTE buffer, LPSTR text, PDWORD bufferLimit) {
//    DWORD textLen = strlen(text);
//    DWORD bufferLen = strlen(buffer);
//
//    if ((bufferLen + textLen + 1)  > *bufferLimit) {
//        *bufferLimit += textLen + 1;
//        buffer = realloc(buffer, *bufferLimit);
//        memcpy(buffer + bufferLen, text, textLen);
//        buffer[bufferLen + textLen] = 0;
//    }
//    else {
//        memcpy(buffer + bufferLen, text, textLen);
//        buffer[bufferLen + textLen] = 0;
//    }
//}
//
//static void printFormat(const char* fText, ...) {
//    va_list args;
//    PBYTE buffer[1024];
//
//    va_start(args, fText);
//
//    vsprintf(buffer, fText, args);
//
//    va_end(args);
//
//    bufferAppend(viewerContent, buffer, &viewerLimit);
//}

static DWORD p = 0;
static INT lineOffset = 0;
//Temp solution
//LPSTR getLine() {
//    LPSTR r = NULL;
//    LPSTR m = strchr(viewerContent + p, '\n');
//
//    if (m) {
//        DWORD sz = m - viewerContent - p;
//        r = malloc(sz);
//        if (r) {
//            memcpy(r, viewerContent + p, sz);
//            p += sz + 1;
//            r[sz] = 0;
//        }
//    }
//
//    return r;
//}

LPSTR binStrToHex(LPSTR input, DWORD inputSize) {
    LPSTR output = (LPSTR)malloc(inputSize * 3 + 1);
    
    for (DWORD i = 0; i < inputSize; i++)
    {
        BYTE t[3];
        sprintf(t, "%X", input[i] & 0xFF);
        if (strlen(t) < 2) {
            output[i * 3] = '0';
            output[i * 3 + 1] = t[0];
            output[i * 3 + 2] = ' ';
        }
        else {
            output[i * 3] = t[0];
            output[i * 3 + 1] = t[1];
            output[i * 3 + 2] = ' ';
        }
    }
    output[inputSize * 3 + 1];

    return output;
}

static void draw_arrow(struct nk_context* ctx, float start_x, float start_y, float end_x, float end_y, struct nk_color color, float line_width, float margin_scale) {
    // Draw main line
    struct nk_command_buffer* buf = nk_window_get_canvas(ctx);

    // Calculate direction vector (dx, dy)
    float dx = start_x - end_x;
    float dy = start_y - end_y;
    float length = sqrtf(dx * dx + dy * dy);
    // Normalize direction vector
    dx /= length;
    dy /= length;

    //float margin_scale = 20.0f;

    float margin1x = start_x + margin_scale * (dx * cosf(-M_PI / 2) - dy * sinf(-M_PI / 2));
    float margin1y = start_y + margin_scale * (dx * sinf(-M_PI / 2) - dy * cosf(-M_PI / 2));

    nk_stroke_line(buf, start_x, start_y, margin1x, margin1y, line_width, color);

    float margin2x = end_x + margin_scale * (dx * cosf(-M_PI / 2) - dy * sinf(-M_PI / 2));
    float margin2y = end_y + margin_scale * (dx * sinf(-M_PI / 2) - dy * cosf(-M_PI / 2));

    nk_stroke_line(buf, end_x, end_y, margin2x, margin2y, line_width, color);

    nk_stroke_line(buf, margin1x, margin1y, margin2x, margin2y, line_width, color);

    start_x = margin2x;
    start_y = margin2y;
    dx = start_x - end_x;
    dy = start_y - end_y;
    length = sqrtf(dx * dx + dy * dy);
    // Normalize direction vector
    dx /= length;
    dy /= length;

    // Arrowhead length and angle
    float arrow_size = 10.0f; // Length of the arrowhead sides
    float arrow_angle = M_PI / 6.0f; // 30 degrees

    // Calculate points for the arrowhead
    float left_x = end_x + arrow_size * (dx * cosf(arrow_angle) - dy * sinf(arrow_angle));
    float left_y = end_y + arrow_size * (dx * sinf(arrow_angle) + dy * cosf(arrow_angle));

    float right_x = end_x + arrow_size * (dx * cosf(-arrow_angle) - dy * sinf(-arrow_angle));
    float right_y = end_y + arrow_size * (dy * cosf(-arrow_angle) + dx * sinf(-arrow_angle));

    // Draw arrowhead lines
    nk_stroke_line(buf, end_x, end_y, left_x, left_y, line_width, color);
    nk_stroke_line(buf, end_x, end_y, right_x, right_y, line_width, color);
}

LPSTR stringNormalize(LPSTR input, DWORD inputLen) {
    LPSTR res = input;

    for (DWORD i = 0; i < inputLen; i++)
    {
        if (!isalnum(input[i]) || !input[i]) {
            input[i] = '.';
        }
    }

    input[inputLen] = 0;

    return res;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, int nCmdShow)
{
    if (__argc < 2) {
        printf("Wrong count of argmunets. Look for usage!\n");
        exit(1);
    }
    
    struct nk_context*  ctx;
    struct nk_colorf    bg;

    WNDCLASSW wc;
    RECT                    rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    DWORD                   style = WS_OVERLAPPEDWINDOW;
    DWORD                   exstyle = WS_EX_APPWINDOW;
    HWND                    wnd;
    BOOL                    running = 1;
    HRESULT                 hr;
    D3D_FEATURE_LEVEL       feature_level;
    DXGI_SWAP_CHAIN_DESC    swap_chain_desc;

    /* Win32 */
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(0);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"NuklearWindowClass";
    RegisterClassW(&wc);

    AdjustWindowRectEx(&rect, style, FALSE, exstyle);
    LPWSTR argvWide = malloc(128);
    mbstowcs(argvWide, __argv[1], strlen(__argv[1]) + 1);
    LPWSTR mainWindowTitle = malloc(256);
    wsprintf(mainWindowTitle, L"%ls (%ls)", L"Lua Bytecode Fucker v0.1", argvWide);
    wnd = CreateWindowExW(exstyle, wc.lpszClassName, mainWindowTitle,
        style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, wc.hInstance, NULL);
    free(argvWide);
    free(mainWindowTitle);

    /* D3D11 setup */
    memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.OutputWindow = wnd;
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc.Flags = 0;

    if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE,
        NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
        &swap_chain, &device, &feature_level, &context)))
    {
        /* if hardware device fails, then try WARP high-performance
           software rasterizer, this is useful for RDP sessions */
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP,
            NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
            &swap_chain, &device, &feature_level, &context);
        assert(SUCCEEDED(hr));
    }
    set_swap_chain_size(WINDOW_WIDTH, WINDOW_HEIGHT);

    /* GUI */
    ctx = nk_d3d11_init(device, WINDOW_WIDTH, WINDOW_HEIGHT, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER);
    /* Load Fonts: if none of these are loaded a default font will be used  */
    /* Load Cursor: if you uncomment cursor loading please hide the cursor */
    {
        struct nk_font_atlas* atlas;
        nk_d3d11_font_stash_begin(&atlas);
        struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "font/DroidSans.ttf", 25, 0);
        /*struct nk_font *robot = nk_font_atlas_add_from_file(atlas, "../../extra_font/Roboto-Regular.ttf", 14, 0);*/
        /*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
        /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../extra_font/ProggyClean.ttf", 12, 0);*/
        /*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../extra_font/ProggyTiny.ttf", 10, 0);*/
        /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../extra_font/Cousine-Regular.ttf", 13, 0);*/
        nk_d3d11_font_stash_end();
        /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
        nk_style_set_font(ctx, &droid->handle);
    }
    
    bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;

    const char* fileName = __argv[1];

    lua_bytecode lbc = { 0 };
    reader* pr = readerInit(fileName);
    lbc.pReader = pr;

    if (!checkHeader(pr)) {
        MessageBoxA(NULL, "It is not luac file!", "ERROR!", MB_OK);
        exit(1);
    }

    lbc.bytecodeVersion = readByte(pr);

    if (lbc.bytecodeVersion != 2) {
        MessageBoxA(NULL, "Only 2.0 bytecode version is supported!", "ERROR!", MB_OK);
        exit(1);
    }
    
    //viewerContent = (PBYTE)malloc(viewerLimit);
    //viewerContent[0] = 0;

    lbc.flags = readUleb128(pr);
    pr->bcFlags = lbc.flags;

    char* chunkname = NULL;
    DWORD chunkname_len = 0;
    if (!(lbc.flags & BCDUMP_F_STRIP)) {
        chunkname_len = readUleb128(pr);
        chunkname = (LPCBYTE)malloc(chunkname_len);
        readBlock(pr, chunkname, 1, chunkname_len);
    }

    for (int proto_id = 0; ; proto_id++) {
        if ((pr->pdata + pr->size) <= pr->current) {
            break;
        }
        readProto(&lbc, proto_id);
    }

    //Main loop
    DWORD selectedProt = 0;
    
    #include "style"
    
    SHORT protoViewerEnabled = FALSE;
    proto* toViewProto;

    SHORT constViewerEnabled = FALSE;
    constant* toViewConst;

    while (running)
    {
        signed int scrollY = 0;
        /* Input */
        MSG msg;
        nk_input_begin(ctx);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            switch (msg.message)
            {
            case WM_QUIT:
                running = 0;
                break;
            case WM_MOUSEWHEEL:
                scrollY = GET_WHEEL_DELTA_WPARAM(msg.wParam);
                break;
            default:
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        nk_input_end(ctx);
        
        /* GUI */
        RECT mainWindowSize;
        if (!wnd) {
            MessageBoxA(NULL, "WND NULL!", "ERROR!", MB_OK);
        }
        if (!ctx) {
            MessageBoxA(NULL, "WND NULL!", "ERROR!", MB_OK);
        }

        GetClientRect(wnd, &mainWindowSize);
        float windowCurrentSize[] = { mainWindowSize.right - mainWindowSize.left , mainWindowSize.bottom - mainWindowSize.top };
        if (nk_begin(ctx, "AkuJlaTools", nk_rect(0, 0, windowCurrentSize[0], windowCurrentSize[1]), 0))
        {
            nk_layout_row(ctx, NK_DYNAMIC, windowCurrentSize[1] * 0.35 - 15, 2, (float []) { 0.6, 0.4 });
            if (nk_group_begin(ctx, "Prototypes", NK_WINDOW_BORDER)) {
                nk_layout_row(ctx, NK_DYNAMIC, 25, 3, (float[]){0.3, 0.2, 0.5});
                
                proto* current = lbc.protos;
                while (current) {
                    if (nk_widget_is_hovered(ctx)) {
                        protosLabelColor = nk_rgb(200, 200, 255);
                    }
                    else {
                        protosLabelColor = nk_rgb(255, 255, 255);
                    }
                    
                    if (nk_widget_is_mouse_clicked(ctx, NK_BUTTON_LEFT)) {
                        selectedProt = current->id;
                        protosLabelColor = nk_rgb(150, 150, 255);
                    }
                    if (nk_widget_is_mouse_clicked(ctx, NK_BUTTON_RIGHT)) {
                        protoViewerEnabled = TRUE;
                        protosLabelColor = nk_rgb(150, 150, 255);
                        toViewProto = current;
                    }

                    if (selectedProt == current->id) {
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(233, 192, 208), "FO:0x%X", current->fileOffset);
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, protosLabelColor, "ID: %d", current->id);
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, protosLabelColor, "Params count: %d", current->paramsCount);
                    }
                    else {
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(136, 192, 208), "FO:0x%X", current->fileOffset);
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, protosLabelColor, "ID: %d", current->id);
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, protosLabelColor, "Params count: %d", current->paramsCount);
                    }
                    current = current->next;
                }

                nk_group_end(ctx);
            }
            if (nk_group_begin(ctx, "Constants", NK_WINDOW_BORDER)) {

                proto* selected = lbc.protos;
                for (DWORD i = 0; i < selectedProt; i++) {
                    selected = selected->next;
                }

                if (!selected->sizeKGC && !selected->sizeKN) {
                    nk_layout_row_dynamic(ctx, 25, 1);
                    nk_label(ctx, "No constants in prototype", NK_TEXT_LEFT);
                }
                else {
                    nk_layout_row(ctx, NK_DYNAMIC, 25, 3, (float[]) { 0.3, 0.25, 0.45 });

                    for (DWORD i = 0; i < selected->sizeKGC; i++) {
                        if(nk_widget_is_mouse_clicked(ctx, NK_BUTTON_LEFT) && selected->kgc[i].type == BCDUMP_KGC_STR) {
                            constViewerEnabled = TRUE;
                            toViewConst = &selected->kgc[i];
                        }
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(136, 192, 208), "FO:0x%X", selected->kgc[i].fileOffset);
                        nk_labelf(ctx, NK_TEXT_LEFT, "%s", selected->kgc[i].constantStr);
                        if (selected->kgc[i].type == BCDUMP_KGC_STR) {
                            if (selected->kgc[i].str) {
                                LPSTR t = stringNormalize(selected->kgc[i].str, selected->kgc[i].stringLen);
                                nk_label(ctx, t, NK_TEXT_LEFT);
                            }
                        }
                        else if (selected->kgc[i].type == BCDUMP_KGC_TAB || selected->kgc[i].type == BCDUMP_KGC_CHILD)
                            nk_label(ctx, "", NK_TEXT_LEFT);
                        else
                            nk_labelf(ctx, NK_TEXT_LEFT, "%d", selected->kgc[i].value);
                    }
                    /*for (DWORD i = 0; i < selected->sizeKN; i++) {
                        nk_labelf(ctx, NK_TEXT_LEFT, "%s", "TYPE");
                        nk_labelf(ctx, NK_TEXT_LEFT, "%s", selected->kn[i].constantStr);
                        nk_labelf(ctx, NK_TEXT_LEFT, "%s", selected->kn[i].constantStr);
                    }*/
                }

                nk_group_end(ctx);
            }
            nk_layout_row(ctx, NK_DYNAMIC, windowCurrentSize[1] * 0.65 - 15, 1, (float[]) { 1.0});
            
            struct nk_scroll scr;
            if (nk_group_scrolled_begin(ctx, &scr, "Bytecode", NK_WINDOW_BORDER)) {
                struct nk_color color = nk_rgba(200, 15, 15, 150);
                DWORD LINE_COUNT = 20;

                lineOffset = scr.y / 25;
                //lineOffset -= scrollY / WHEEL_DELTA;

                if (lineOffset < 0) lineOffset = 0;

                proto* selected = lbc.protos;
                for (DWORD i = 0; i < selectedProt; i++) {
                    selected = selected->next;
                }

                //for (DWORD i = lineOffset; i < selected->sizeBC && i < (LINE_COUNT + lineOffset); i++)
                for (DWORD i = 0, MARGIN_FLAG = 0; i < selected->sizeBC; i++) {
                    nk_layout_row(ctx, NK_DYNAMIC, 25, 7, (float[]) { 0.2, 0.1, 0.2, 0.05, 0.05, 0.05, 0.05 });

                    if (nk_widget_is_hovered(ctx)) {
                        selected->bc[i].r = 200;
                        selected->bc[i].g = 200;
                        selected->bc[i].blue = 255;
                    }
                    else {
                        selected->bc[i].r = 255;
                        selected->bc[i].g = 255;
                        selected->bc[i].blue = 255;
                    }

                    nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(136, 192, 208), "FO:0x%X", selected->bc[i].fileOffset);

                    nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgba(selected->bc[i].r, selected->bc[i].g, selected->bc[i].blue, 255), "%d", selected->bc[i].opPc);
                    if (
                        (selected->bc[i].opcodeHex == 0x58) && 
                        (0 < (selected->bc[i].jmpAddr + (signed int)(selected->bc[i].opPc))) && 
                        ( (selected->bc[i].jmpAddr + (signed int)(selected->bc[i].opPc)) <= (signed int)(selected->sizeBC) )
                       ) 
                    {
                        nk_labelf(ctx, NK_TEXT_LEFT, "%d", (selected->bc[i].jmpAddr + (signed int)(selected->bc[i].opPc)));
                        struct nk_vec2 m = nk_widget_position(ctx);
                        struct nk_rect s = nk_widget_bounds(ctx);
                        DWORD t = selected->bc[i].jmpAddr;
                        if ((selected->bc[i].jmpAddr + 1) < selected->sizeBC) {
                            if (MARGIN_FLAG) {
                                draw_arrow(ctx, s.x - 3, m.y, s.x - 3, m.y + 25 * (selected->bc[i].jmpAddr + 1), color, 2.0f, 40.0f);
                                MARGIN_FLAG = FALSE;
                            }
                            else {
                                draw_arrow(ctx, s.x - 3, m.y, s.x - 3, m.y + 25 * (selected->bc[i].jmpAddr + 1), color, 2.0f, 20.0f);
                                MARGIN_FLAG = TRUE;
                            }
                        }
                    }

                    if (nk_widget_is_mouse_clicked(ctx, NK_BUTTON_LEFT)) {
                        ShellExecute(0, 0, L"some.html", 0, 0, SW_SHOW);
                    }

                    if (selected->bc[i].opcodeHex >= 97) {
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgba(130, 130, 130, 255), "%s<0x%X>", selected->bc[i].opName, selected->bc[i].opcodeHex);
                    }
                    else {
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgba(129, 161, 193, 255), "%s<0x%X>", selected->bc[i].opName, selected->bc[i].opcodeHex);
                    }

                    nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(180, 142, 173), "%d", selected->bc[i].a);
                    nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(180, 142, 173), "%d", selected->bc[i].b);
                    nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(180, 142, 173), "%d", selected->bc[i].c);
                    nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(180, 142, 173), "%d", selected->bc[i].d);
                    /*if (selected->bc[i].opcodeHex == 0x27) {
                        nk_labelf_colored(ctx, NK_TEXT_LEFT, nk_rgb(180, 180, 180), "%s", selected->kgc[selected->bc[i].d].str);
                    }*/
                }

                nk_group_end(ctx);
            }
            //if (nk_group_begin_titled(ctx, "#HEX", "Hex", NK_WINDOW_BORDER)) {
            //    nk_layout_row_dynamic(ctx, 25, 1);
            //    /*BYTE* content = binStrToHex(pr->pdata, pr->size);
            //    
            //    nk_label(ctx, content, NK_TEXT_LEFT);
            //    free(content);*/
            //    nk_group_end(ctx);
            //}
        }
        nk_end(ctx);

        if (constViewerEnabled) {
            if (constViewerEnabled = nk_begin(ctx, "Const viewer", nk_rect(windowCurrentSize[0] / 2 - 200 / 2, windowCurrentSize[1] / 2 - 200 / 2, 400, 200, 200), NK_WINDOW_BORDER | NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
                if (!nk_window_is_active(ctx, "Const viewer")) constViewerEnabled = FALSE;

                struct nk_vec2 windowSize = nk_window_get_size(ctx);
                struct nk_rect t = nk_window_get_bounds(ctx);
                
                nk_layout_row_dynamic(ctx, 0, 1);
                //nk_text(ctx, toViewConst->str, toViewConst->stringLen, NK_TEXT_LEFT);
                struct nk_text s;
                s.background = nk_rgba(200, 0, 0, 255);      // Background color of text area
                s.text = nk_rgba(0, 200, 0, 255);            // Text color
                s.padding = nk_vec2(0, 0);             // Padding around text

                // Get Nuklear drawing canvas and wrap text in defined rectangle
                nk_widget_text_wrap(nk_window_get_canvas(ctx), nk_rect(t.x + 50, t.y + 50, t.w - 100, t.h),
                    toViewConst->str, toViewConst->stringLen, &s, ctx->style.font);
                
                //nk_label_wrap(ctx, "This is a long label text that will automatically wrap within the container width.");
                //nk_label(ctx, toViewConst->str, NK_TEXT_LEFT);

                /*LPSTR hex = binStrToHex(toViewConst->str, toViewConst->stringLen);
                nk_label(ctx, hex, NK_TEXT_LEFT);
                free(hex);*/
            }
            nk_end(ctx);
        }

        if (protoViewerEnabled) {
            if (protoViewerEnabled = nk_begin(ctx, "Proto viewer", nk_rect(windowCurrentSize[0]/2 - 400/2, windowCurrentSize[1]/2 - 500/2, 400, 500), NK_WINDOW_BORDER | NK_WINDOW_CLOSABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
                if (!nk_window_is_active(ctx, "Proto viewer")) protoViewerEnabled = FALSE;
                nk_layout_row_dynamic(ctx, 30, 2);  // Two columns with 30px height

                // Use a grid layout to align labels and values
                nk_label(ctx, "File Offset:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "0x%X", toViewProto->fileOffset);

                nk_label(ctx, "Proto Length:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->protoLen);

                nk_label(ctx, "ID:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->id);

                nk_label(ctx, "Flags:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->flags);

                nk_label(ctx, "Params Count:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->paramsCount);

                nk_label(ctx, "Frame Size:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->frameSize);

                nk_label(ctx, "Size UV:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->sizeUV);

                nk_label(ctx, "Size KGC:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->sizeKGC);

                nk_label(ctx, "Size KN:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->sizeKN);

                nk_label(ctx, "Size BC:", NK_TEXT_LEFT);
                nk_labelf(ctx, NK_TEXT_LEFT, "%u", toViewProto->sizeBC);
            }
            nk_end(ctx);
        }

        /* Draw */
        ID3D11DeviceContext_ClearRenderTargetView(context, rt_view, &bg.r);
        ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rt_view, NULL);
        nk_d3d11_render(context, NK_ANTI_ALIASING_ON);
        hr = IDXGISwapChain_Present(swap_chain, 1, 0);
        if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
            /* to recover from this, you'll need to recreate device and all the resources */
            MessageBoxW(NULL, L"D3D11 device is lost or removed!", L"Error", 0);
            break;
        } 
        else if (hr == DXGI_STATUS_OCCLUDED) {
            /* window is not visible, so vsync won't work. Let's sleep a bit to reduce CPU usage */
            Sleep(10);
        }
        assert(SUCCEEDED(hr));
    }

    ID3D11DeviceContext_ClearState(context);
    nk_d3d11_shutdown();
    ID3D11RenderTargetView_Release(rt_view);
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    IDXGISwapChain_Release(swap_chain);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    //Cleanup //TODO
    readerFree(pr);
    //free(viewerContent);

	return EXIT_SUCCESS;
}
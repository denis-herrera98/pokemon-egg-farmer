#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <unistd.h>
#include <string.h>

// gcc main.c -o main -lX11 -lXtst -ltesseract -lleptonica

#define POSX   0
#define POSY   0
#define HEIGHT 200
#define WIDTH  600
#define BORDER 15 

// "Banette frisked Pelipper and found anitem!"
// [Battle|Banette frisked Pelipper and found anitem!
// [Battle|Banette frisked Pelipper and found anitem!

// "Banette le ha robado P.Bella al Pelipper salvaje"
// Banette le ha robado P. Bellaal Pelipper salvaje!

// "Banette le ha robado Roca Lluvia al Pelipper salvaje"
// Banette le ha robado Roca Lluviaal Pelipper salvaje!

// "Escapas sin problemas"
// Escapas sin problemas!
//
enum Actions {
    BLOCK,   
    ITEM_FOUND,
    UNWANTED_ITEM,
    ESCAPE,
    START_COMBAT
};

void save_image_as_ppm(const char *filename, XImage *image) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    // Write PPM header
    fprintf(file, "P6\n%d %d\n255\n", image->width, image->height);

    // Write image data
    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            unsigned long pixel = XGetPixel(image, x, y);
            fputc((pixel >> 16) & 0xFF, file); 
            fputc((pixel >> 8) & 0xFF, file);  
            fputc(pixel & 0xFF, file);         
        }
    }

    fclose(file);
}

int captureScreen(Display *display){

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    int screenHeight = DisplayHeight(display, screen);

    XImage *image = XGetImage(display, root, POSX, 830, WIDTH, HEIGHT, AllPlanes, ZPixmap);
    if (image == NULL) {
        fprintf(stderr, "Failed to get image\n");
        XCloseDisplay(display);
        return 1;
    }

    save_image_as_ppm("screenshot.ppm", image);

    XDestroyImage(image);
    return 0;

}

char *readImageText(){
    // Initialize Tesseract API
    TessBaseAPI *api = TessBaseAPICreate();
    if (TessBaseAPIInit3(api, NULL, "eng")) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        TessBaseAPIDelete(api);
        return NULL;
    }

    // Read the image using Leptonica
    struct Pix *pixImage = pixRead("screenshot.ppm");
    if (!pixImage) {
        fprintf(stderr, "Could not read image.\n");
        TessBaseAPIDelete(api);
        return NULL;
    }

    // Set the image to Tesseract API
    TessBaseAPISetImage2(api, pixImage);

    char *text = TessBaseAPIGetUTF8Text(api);
    size_t length = strlen(text);
    char *copy = malloc((length + 1) * sizeof(char));

    if (copy == NULL) {
        return NULL; // Handle malloc failure
    }
    strcpy(copy, text);

    if (text) {
    //    printf("OCR Output:\n%s", text);
        TessDeleteText(text);
    } else {
        fprintf(stderr, "Failed to get OCR text.\n");
    }

    // Cleanup
    pixDestroy(&pixImage);
    TessBaseAPIDelete(api);
    return copy;
}

enum Actions getActionToExecute(char *text){
    
    printf("OCR Output:\n\n %s", text);
    char* foundItem = "found";
    char* unwantedItem1 = "Roca";
    char* unwantedItem2 = "P.";
    char* escape = "Escapas";
    char* horde = "horde";

    if (strstr(text, foundItem) != NULL) {
        printf("\nA ITEM WAS FOUND!!\n");
        return BLOCK;
    } 

    if (strstr(text, unwantedItem1) != NULL | strstr(text, unwantedItem2) != NULL) {
        printf("\nGOT UNWATED ITEM :(\n");
        return UNWANTED_ITEM;
    } 

//    if (strstr(text, escape) != NULL) {
//        printf("\nESCAPE!! :(\n");
//        return ESCAPE;
//    } 
//
    if (strstr(text, horde) != NULL & strstr(text, foundItem) == NULL) {
        printf("\nHORDE!! :(\n");
        return ESCAPE;
    } 

    return 0;
}

void simulate_keypress(Display *display, KeySym key) {
    KeyCode keycode = XKeysymToKeycode(display, key);
    
    // Press the key
    XTestFakeKeyEvent(display, keycode, True, 0);
    XFlush(display);

    // Release the key
    XTestFakeKeyEvent(display, keycode, False, 0);
    XFlush(display);
}

void executeAction(Display *display, enum Actions action) {
    switch (action) {
        case BLOCK:
            printf("Starting block action.\n");
            simulate_keypress(display, XK_Z);
            simulate_keypress(display, XK_Right); 
            simulate_keypress(display, XK_Z);
            simulate_keypress(display, XK_Z);
            printf("Block action done.\n");
            break;
        case UNWANTED_ITEM:
            printf("Starting unwanted action.\n");
            printf("Unwanted action done.\n");
            break;
        case ESCAPE:
            printf("Starting escape action.\n");
            simulate_keypress(display, XK_Down); 
            simulate_keypress(display, XK_Right); 
            simulate_keypress(display, XK_Z); 
            printf("Escape action done.\n");
            break;
        case START_COMBAT:
            printf("Starting combat action.\n");
            simulate_keypress(display, XK_1);
            printf("Start combat action done.\n");
            break;
        default:
            printf("Unknown action.\n");
            break;
    }
}

int main() {

    Display *display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    while (1) {
        int screenshotSuccess = captureScreen(display);
        if (screenshotSuccess != 0) {
            return 1;
            break;
        }

        char* text = readImageText();
        if (text == NULL) {
            return 1;
            break;
        }

//        enum Actions action = getActionToExecute(text);
//        executeAction(display, action);

//        simulate_keypress(display, XK_1); // 1 1 1
//        simulate_keypress(display, XK_Z); // Z Z Z
//        simulate_keypress(display, XK_Up); // UP UP UP
//        simulate_keypress(display, XK_Down); // DOWN DOWN DOWN
//        simulate_keypress(display, XK_Left); // LEFT LEFT LEFT
//        simulate_keypress(display, XK_Right); // RIGHT RIGHT RIGHT
//
        sleep(5);

    }

    // strcmp(str1, str2);
    //   // Simulate pressing the Up Arrow key
    

    XCloseDisplay(display);

    return 0;
}



// CERCA  : Z, RIGHT, Z, Z
// TIRAR  : Z, RIGHT, Z -> TO A VALID TARGET
// ESCAPE : DOWN, RIGHT, Z
// #1     : Z, LEFT, Z, Z
// #2     : Z, Z, DOWN, Z
// #3     : Z, Z, LEFT, Z
// #4     : Z, Z, LEFT, LEFT, Z
// #5     : Z, Z, LEFT, LEFT, DOWN, Z


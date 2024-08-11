#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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
    START_COMBAT,
    ATTACK,
    CHANGE_POKEMON,
};

bool blockCompleted = false;
bool isInCombat = false;
bool wasItemFound = false;
bool firstTimeAttacking = true;
bool gotItem = false;
 
void restartState(){ 
    blockCompleted = false;
    isInCombat = false;
    wasItemFound = false;
    firstTimeAttacking = true;
    gotItem = false;
}

Window find_window_by_name(Display *display, Window root, const char *window_name) {
    Window parent;
    Window *children;
    unsigned int nchildren;
    char *name;

    // Get the list of child windows
    if (XQueryTree(display, root, &root, &parent, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; i++) {
            // Get the window name
            if (XFetchName(display, children[i], &name) > 0) {
                // Compare the name
                printf("%s\n", name);
                if (name && strcmp(name, window_name) == 0) {
                    XFree(name);
                    return children[i]; // Return the matched window ID
                }
                XFree(name);
            }

            // Recursively search in child windows
            Window w = find_window_by_name(display, children[i], window_name);
            if (w) {
                return w;
            }
        }

        if (children) {
            XFree(children); // Free the list of child windows
        }
    }

    return 0; // Return 0 if no matching window is found
}

void save_image_as_ppm(const char *filename, XImage *image) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    fprintf(file, "P6\n%d %d\n255\n", image->width, image->height);

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

int captureScreen(Display *display, int posX, int posY, unsigned width, unsigned height, char* imgName){

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    int screenHeight = DisplayHeight(display, screen);

    XImage *image = XGetImage(display, root, posX, posY, width, height , AllPlanes, ZPixmap);
    if (image == NULL) {
        fprintf(stderr, "Failed to get image\n");
        XCloseDisplay(display);
        return 1;
    }

    save_image_as_ppm(imgName, image);

    XDestroyImage(image);
    return 0;

}

char *readImageText(char* imgName){
    // Initialize Tesseract API
    TessBaseAPI *api = TessBaseAPICreate();
    if (TessBaseAPIInit3(api, NULL, "eng")) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        TessBaseAPIDelete(api);
        return NULL;
    }

    // Read the image using Leptonica
    struct Pix *pixImage = pixRead(imgName);
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
        printf("\nOCR Output:\n%s", text);
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
    
    char* foundItem = "found";
    char* unwantedItem1 = "Roca";
    char* unwantedItem2 = "P.";
    char* escape = "Escapas";
    char* horde = "horde"; 
    char* stoleItem = "robado";

    if (!wasItemFound && isInCombat) {
        wasItemFound = strstr(text, foundItem) != NULL;
    }

    if (isInCombat && strstr(text, stoleItem) != NULL) {
        printf("\nITEM WAS STOLEN, LEAVING...\n");
        gotItem = true;
        return ESCAPE;
    }

    if (!isInCombat) {
        printf("\nSHOULD START COMBAT!! :(\n");
        return START_COMBAT;
    } 

        printf("\nblock %b, wasItemFound %b, inCombat %b\n", blockCompleted, wasItemFound, isInCombat);
        printf("\nblockCompleted = %d, wasItemFound = %d, isInCombat = %d\n", blockCompleted, wasItemFound, isInCombat);
    if (blockCompleted == 0 && wasItemFound && isInCombat) {
        printf("\nA ITEM WAS FOUND!!\n");
        return BLOCK;
    } 

    if (isInCombat && wasItemFound == false) {
        printf("\nHORDE! !:(\n");
        return ESCAPE;
    } 

    if (isInCombat && wasItemFound) {
        printf("\nHORDE! !:(\n");
        return ATTACK;
    } 

    if (strstr(text, unwantedItem1) != NULL | strstr(text, unwantedItem2) != NULL) {
        printf("\nGOT UNWATED ITEM :(\n");
        printf("\nGOT UNWATED ITEM :(\n");
        printf("\nGOT UNWATED ITEM :(\n");
        return UNWANTED_ITEM;
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
    sleep(1);
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
            blockCompleted = true;
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
            restartState();
            break;
        case START_COMBAT:
            printf("Starting combat action.\n");
            simulate_keypress(display, XK_1);
            printf("Start combat action done.\n");
            isInCombat = true;
            break;
        case CHANGE_POKEMON:
            printf("Starting combat action.\n");
            simulate_keypress(display, XK_Down); 
            simulate_keypress(display, XK_Right); 
            simulate_keypress(display, XK_Z); 
            printf("Start combat action done.\n");
            isInCombat = true;
            break;
        case ATTACK:
            printf("Starting attack action.\n");
            simulate_keypress(display, XK_Z);
            if (firstTimeAttacking){
                simulate_keypress(display, XK_Left);
                firstTimeAttacking = false;
            }
            simulate_keypress(display, XK_Z);
            simulate_keypress(display, XK_Z);
            printf("attack action done.\n");
            break;
        default:
            printf("Unknown action.\n");
            break;
    }
}

int main() {

    Display *display = XOpenDisplay(NULL);
//    char* windowName = "РokеМMO";
//    Window window = find_window_by_name(display, DefaultRootWindow(display), "РokеМMO");

 //   if (window == 0) {
 //       fprintf(stderr, "Unable to find %s\n window", windowName);
 //       return 1;
 //   }

    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    sleep(3);
    while (1) {

        int fightScreenShot = captureScreen(display, 300, 640, WIDTH, HEIGHT - 20, "readyToPlay.ppm");
        if (fightScreenShot != 0) {
            return 1;
            break;
        }

        char* fightText = readImageText("readyToPlay.ppm");
        if (fightText == NULL) {
            return 1;
            break;
        }

        bool isReadyToFight = strstr(fightText, "FIGHT BAG") != NULL;

        if ((isReadyToFight || !isInCombat) && gotItem == false) {

            int screenshotSuccess = captureScreen(display, POSX, 830, WIDTH, HEIGHT, "chatScreenshot.ppm");
            if (screenshotSuccess != 0) {
                return 1;
                break;
            }

            char* text = readImageText("chatScreenshot.ppm");
            if (text == NULL) {
                return 1;
                break;
            }

            printf("LOOKING FOR ACTIONS TO EXECUTE");
            enum Actions action = getActionToExecute(text);
            executeAction(display, action);
            free(text);
            free(fightText);

        } else {
          printf("SKIPPING THIS TURN");
        }   

        sleep(3);
    }

    XCloseDisplay(display);

    return 0;
}

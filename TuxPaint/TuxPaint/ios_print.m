//
//  macosx_print.m
//  Tux Paint
//
//  Created by Darrell Walisser on Sat Mar 15 2003.
//  Modified by Martin Fuhrer 2007.
//  Copyright (c) 2007 Darrell Walisser, Martin Fuhrer.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  (See COPYING.txt)
//
//  $Id: macosx_print.m,v 1.8 2008/02/05 06:35:11 mfuhrer Exp $
//

#import "ios_print.h"
#import "wrapperdata.h"   //dailin
//#import <Cocoa/Cocoa.h>
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <AssetsLibrary/AssetsLibrary.h>
extern WrapperData macosx;
NSData* printData = nil;
extern int do_open1(SDL_Surface ** thumbs, int num_files_in_dirs);
// this object presents the image to the printing layer
@interface ImageView : UIView
{
    UIImage* _image;
}
- (void) setImage:(UIImage*)image;
@end

@implementation ImageView

- (void) setImage:(UIImage*)image
{
    _image = [ image retain ];
}

- (void) drawRect:(CGRect)rect
{
//    [ _image compositeToPoint: CGMakePoint( 0, 0 ) operation: UICompositeCopy ];
}

- (BOOL) scalesWhenResized
{
	return YES;
}

@end

// this object waits for the print dialog to go away
@interface ModalDelegate : NSObject
{
    BOOL	_complete;
    BOOL    _wasOK;
}
- (id)   init;
- (BOOL) wait;
- (void) reset;
- (BOOL) wasOK;
@end

@implementation ModalDelegate

- (id) init
{
    self = [ super init ];
    _complete = NO;
    _wasOK = NO;
    return self;
}

- (BOOL) wait
{
//    while (!_complete) {
//        UIEvent *event;
//        event = [ NSApp nextEventMatchingMask:NSAnyEventMask
//                        untilDate:[ NSDate distantFuture ]
//                        inMode: NSDefaultRunLoopMode dequeue:YES ];
//        [ NSApp sendEvent:event ];
//    }
    
    return [ self wasOK ];
}

- (void) reset
{
    _complete = NO;
    _wasOK = NO;
}

- (BOOL) wasOK
{
    return _wasOK;
}

//- (void)printDidRun:(NSPrintOperation *)printOperation
//            success:(BOOL)success contextInfo:(void *)contextInfo
//{
//    _complete = YES;
//    _wasOK = success;
//}
//
//- (void)pageLayoutEnded:(UIPageLayout *)pageLayout
//             returnCode:(int)returnCode contextInfo:(void *)contextInfo
//{
////    _complete = YES;dailin
////    _wasOK = returnCode == OKButton;
//}

@end

void DefaultPrintSettings( const SDL_Surface *surface, UIPrintInfo *printInfo )
{
    if( surface->w > surface->h )
        [ printInfo setOrientation:UIPrintInfoOrientationLandscape ];
    else
        [ printInfo setOrientation:UIPrintInfoOrientationPortrait ];
    
	[ printInfo setHorizontallyCentered:true ];
	[ printInfo setVerticallyCentered:true ];
//	[ printInfo setVerticalPagination:NSFitPagination ]; dailin
//	[ printInfo setHorizontalPagination:NSFitPagination ];    
}

UIPrintInfo* LoadPrintInfo( const SDL_Surface *surface )
{
//    NSUserDefaults*   standardUserDefaults;
//    UIPrintInfo*      printInfo;
//    NSData*           printData = nil;
//    static BOOL       firstTime = YES;   
//    
//    standardUserDefaults = [ NSUserDefaults standardUserDefaults ];
//    
//    if( standardUserDefaults ) 
//        printData = [ standardUserDefaults dataForKey:@"PrintInfo" ];
//   
//    if( printData )
//        printInfo = (UIPrintInfo*)[ UIUnarchiver unarchiveObjectWithData:printData ];
//    else
//    {
//        printInfo = [ UIPrintInfo sharedPrintInfo ];
//        if( firstTime == YES ) 
//        {
//            DefaultPrintSettings( surface, printInfo );
//            firstTime = NO;
//        }
//    }
//    
//    return printInfo;
    return NULL;
}

void SavePrintInfo( UIPrintInfo* printInfo )
{
//    NSUserDefaults*   standardUserDefaults;
//    NSData*           printData = nil;
//    
//    printData = [ NSArchiver archivedDataWithRootObject:printInfo ];
//    standardUserDefaults = [ NSUserDefaults standardUserDefaults ];
//    
//    if( standardUserDefaults ) 
//        [ standardUserDefaults setObject:printData forKey:@"PrintInfo" ];
}

int DisplayPageSetup( const SDL_Surface * surface )
{
//	UIPageLayout*     pageLayout;
//	UIPrintInfo*      printInfo;
//	ModalDelegate*    delegate;
//    BOOL              result;
//    
//    macosx.cocoaKeystrokes = 1;
//    
//    printInfo = LoadPrintInfo( surface );
//
//    delegate = [ [ [ ModalDelegate alloc ] init ] autorelease ];
//	pageLayout = [ NSPageLayout pageLayout ];
//	[ pageLayout beginSheetWithPrintInfo:printInfo 
//                 modalForWindow:[ NSApp mainWindow ]
//                 delegate:delegate
//                 didEndSelector:@selector(pageLayoutEnded:returnCode:contextInfo:)
//                 contextInfo:nil ];
//	
//	result = [ delegate wait ];
//    SavePrintInfo( printInfo );
//    
//    macosx.cocoaKeystrokes = 0;
//    
//    return (int)( result );
    return 0;
}

const char* SurfacePrint( SDL_Surface *surface, int showDialog )
{
//    UIImage*          image;
//    ImageView*        printView;
//    UIWindow*         printWindow;
//    UIPrintOperation* printOperation;
//    UIPrintInfo*      printInfo;
//    ModalDelegate*    delegate;
//    BOOL              ok = YES;
//
//    // check if printers are available
//    NSArray* printerNames = [UIPrinter printerNames];
//    if( [printerNames count] == 0 && !showDialog)
//        return "No printer is available.  Run Tux Paint in window mode (not fullscreen), and select File > Print... to choose a printer.";
//    
//    // create image for surface
//    image = CreateImage( surface );
//    if( image == nil )
//        return "Could not create a print image.";
//	
//    // create print control objects
//    printInfo = LoadPrintInfo( surface );
//     
//	NSRect pageRect = [ printInfo imageablePageBounds ];
//	NSSize pageSize = pageRect.size;
//	NSPoint pageOrigin = pageRect.origin;
//	
//	[ printInfo setTopMargin:pageOrigin.y ];
//	[ printInfo setLeftMargin:pageOrigin.x ];
//	[ printInfo setRightMargin:pageOrigin.x ];
//	[ printInfo setBottomMargin:pageOrigin.y ];
//	
//	float surfaceRatio = (float)( surface->w ) / (float)( surface->h );
//	float pageRatio = pageSize.width / pageSize.height;
//	
//	NSSize imageSize = pageSize;
//	if( pageRatio > surfaceRatio )   // wide page
//	    imageSize.width = surface->w * pageSize.height / surface->h;
//	else  // tall page
//		imageSize.height = surface->h * pageSize.width / surface->w;
//    
//	// create print view
//    printView = [ [ [ ImageView alloc ] initWithFrame: NSMakeRect( 0, 0, imageSize.width, imageSize.height ) ] autorelease ];
//	if (printView == nil)
//        return "Could not create a print view.";
//    
//	[ image setSize:imageSize ];
//    [ printView setImage:image ];
//	
//    // run printing
//    printOperation = [ NSPrintOperation printOperationWithView:printView printInfo:printInfo ];
//    [ printOperation setShowPanels:showDialog ];
//    
//    macosx.cocoaKeystrokes = 1;
//    delegate = [ [ [ ModalDelegate alloc ] init ] autorelease ];
//    [ printOperation runOperationModalForWindow:[ NSApp mainWindow ]
//        delegate:delegate didRunSelector:@selector(printDidRun:success:contextInfo:) contextInfo:nil ];
//    
//    ok = [ delegate wait ];
//        
//    macosx.cocoaKeystrokes = 0;
//    
//    SavePrintInfo( printInfo );
//    [ image release ];
	   
    return NULL;
}

void SurfaceSave(SDL_Surface *surface, const char *name)
{
    GLint backingWidth, backingHeight;
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);
    
    
    
    NSInteger x = (1024 - surface->w)/2, y = 768 - surface->h, width = surface->w, height = surface->h;
    
    NSInteger dataLength = width * height * 4;
    
    GLubyte *data = (GLubyte*)malloc(dataLength * sizeof(GLubyte));
    
    
    
    // Read pixel data from the framebuffer
    
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    
    
    // Create a CGImage with the pixel data
    
    // If your OpenGL ES content is opaque, use kCGImageAlphaNoneSkipLast to ignore the alpha channel
    
    // otherwise, use kCGImageAlphaPremultipliedLast
    
    CGDataProviderRef ref = CGDataProviderCreateWithData(NULL, data, dataLength, NULL);
    
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    
    CGImageRef iref = CGImageCreate(width, height, 8, 32, width * 4, colorspace, kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast,ref, NULL, true, kCGRenderingIntentDefault);
    
    
    
    // OpenGL ES measures data in PIXELS
    
    // Create a graphics context with the target size measured in POINTS
    
    NSInteger widthInPoints, heightInPoints;
    
    if (NULL != UIGraphicsBeginImageContextWithOptions) {
        
        // On iOS 4 and later, use UIGraphicsBeginImageContextWithOptions to take the scale into consideration
        
        // Set the scale parameter to your OpenGL ES view's contentScaleFactor
        
        // so that you get a high-resolution snapshot when its value is greater than 1.0
        
        CGFloat scale =1;
        
        widthInPoints = width / scale;
        
        heightInPoints = height / scale;
        
        UIGraphicsBeginImageContextWithOptions(CGSizeMake(widthInPoints, heightInPoints), NO, scale);
        
    }
    
    else {
        
        // On iOS prior to 4, fall back to use UIGraphicsBeginImageContext
        
        widthInPoints = width;
        
        heightInPoints = height;
        
        UIGraphicsBeginImageContext(CGSizeMake(widthInPoints, heightInPoints));
        
    }
    
    
    
    CGContextRef cgcontext = UIGraphicsGetCurrentContext();
    
    
    
    // UIKit coordinate system is upside down to GL/Quartz coordinate system
    
    // Flip the CGImage by rendering it to the flipped bitmap context
    
    // The size of the destination area is measured in POINTS
    
    CGContextSetBlendMode(cgcontext, kCGBlendModeCopy);
    
    CGContextDrawImage(cgcontext, CGRectMake(0.0, 0.0, widthInPoints, heightInPoints), iref);
    
    
    
    // Retrieve the UIImage from the current context
    
    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    
    
    
    UIGraphicsEndImageContext();
    
    
    
    // Clean up
    
    free(data);
    
    CFRelease(ref);
    
    CFRelease(colorspace);
    
    CGImageRelease(iref);
    
    
    
	NSString *string_name = [[[NSString alloc] initWithCString:(const char*)name encoding:NSASCIIStringEncoding] autorelease];
    [UIImagePNGRepresentation(image) writeToFile:string_name atomically:YES];  
    
}

void SurfaceSaveToPHOTO(SDL_Surface *surface)
{
    GLint backingWidth, backingHeight;
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);
    
    
    
    NSInteger x = (1024 - surface->w)/2, y = 768 - surface->h, width = surface->w, height = surface->h;
    
    NSInteger dataLength = width * height * 4;
    
    GLubyte *data = (GLubyte*)malloc(dataLength * sizeof(GLubyte));
    
    
    
    // Read pixel data from the framebuffer
    
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    
    glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    
    
    // Create a CGImage with the pixel data
    
    // If your OpenGL ES content is opaque, use kCGImageAlphaNoneSkipLast to ignore the alpha channel
    
    // otherwise, use kCGImageAlphaPremultipliedLast
    
    CGDataProviderRef ref = CGDataProviderCreateWithData(NULL, data, dataLength, NULL);
    
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    
    CGImageRef iref = CGImageCreate(width, height, 8, 32, width * 4, colorspace, kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast,ref, NULL, true, kCGRenderingIntentDefault);
    
    
    
    // OpenGL ES measures data in PIXELS
    
    // Create a graphics context with the target size measured in POINTS
    
    NSInteger widthInPoints, heightInPoints;
    
    if (NULL != UIGraphicsBeginImageContextWithOptions) {
        
        // On iOS 4 and later, use UIGraphicsBeginImageContextWithOptions to take the scale into consideration
        
        // Set the scale parameter to your OpenGL ES view's contentScaleFactor
        
        // so that you get a high-resolution snapshot when its value is greater than 1.0
        
        CGFloat scale =1;
        
        widthInPoints = width / scale;
        
        heightInPoints = height / scale;
        
        UIGraphicsBeginImageContextWithOptions(CGSizeMake(widthInPoints, heightInPoints), NO, scale);
        
    }
    
    else {
        
        // On iOS prior to 4, fall back to use UIGraphicsBeginImageContext
        
        widthInPoints = width;
        
        heightInPoints = height;
        
        UIGraphicsBeginImageContext(CGSizeMake(widthInPoints, heightInPoints));
        
    }
    
    
    
    CGContextRef cgcontext = UIGraphicsGetCurrentContext();
    
    
    
    // UIKit coordinate system is upside down to GL/Quartz coordinate system
    
    // Flip the CGImage by rendering it to the flipped bitmap context
    
    // The size of the destination area is measured in POINTS
    
    CGContextSetBlendMode(cgcontext, kCGBlendModeCopy);
    
    CGContextDrawImage(cgcontext, CGRectMake(0.0, 0.0, widthInPoints, heightInPoints), iref);
    
    
    
    // Retrieve the UIImage from the current context
    
    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    
    
    
    UIGraphicsEndImageContext();
    
    
    
    // Clean up
    
    free(data);
    
    CFRelease(ref);
    
    CFRelease(colorspace);
    
    CGImageRelease(iref);
    
    
    
    UIImageWriteToSavedPhotosAlbum(image, nil, nil, nil);
}


static SDL_Surface* Create_SDL_Surface_From_CGImage(CGImageRef image_ref)
{
    /* This code is adapted from Apple's Documentation found here:
     * http://developer.apple.com/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/index.html
     * Listing 9-4††Using a Quartz image as a texture source.
     * Unfortunately, this guide doesn't show what to do about
     * non-RGBA image formats so I'm making the rest up.
     * All this code should be scrutinized.
     */
    
    size_t w = CGImageGetWidth(image_ref);
    size_t h = CGImageGetHeight(image_ref);
    CGRect rect = {{0, 0}, {w, h}};
    
    CGImageAlphaInfo alpha = CGImageGetAlphaInfo(image_ref);
    //size_t bits_per_pixel = CGImageGetBitsPerPixel(image_ref);
    size_t bits_per_component = 8;
    
    SDL_Surface* surface;
    Uint32 Amask;
    Uint32 Rmask;
    Uint32 Gmask;
    Uint32 Bmask;
    
    CGContextRef bitmap_context;
    CGBitmapInfo bitmap_info;
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    
    if (alpha == kCGImageAlphaNone ||
        alpha == kCGImageAlphaNoneSkipFirst ||
        alpha == kCGImageAlphaNoneSkipLast) {
        bitmap_info = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host; /* XRGB */
        Amask = 0x00000000;
    } else {
        /* kCGImageAlphaFirst isn't supported */
        //bitmap_info = kCGImageAlphaFirst | kCGBitmapByteOrder32Host; /* ARGB */
        bitmap_info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host; /* ARGB */
        Amask = 0xFF000000;
    }
    
    Rmask = 0x00FF0000;
    Gmask = 0x0000FF00;
    Bmask = 0x000000FF;
    
    surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, Rmask, Gmask, Bmask, Amask);
    if (surface)
    {
        // Sets up a context to be drawn to with surface->pixels as the area to be drawn to
        bitmap_context = CGBitmapContextCreate(
                                               surface->pixels,
                                               surface->w,
                                               surface->h,
                                               bits_per_component,
                                               surface->pitch,
                                               color_space,
                                               bitmap_info
                                               );
        
        // Draws the image into the context's image_data
        CGContextDrawImage(bitmap_context, rect, image_ref);
        
        CGContextRelease(bitmap_context);
        
        // FIXME: Reverse the premultiplied alpha
        if ((bitmap_info & kCGBitmapAlphaInfoMask) == kCGImageAlphaPremultipliedFirst) {
            int i, j;
            Uint8 *p = (Uint8 *)surface->pixels;
            for (i = surface->h * surface->pitch/4; i--; ) {
#if __LITTLE_ENDIAN__
                Uint8 A = p;
                if (A) {
                    for (j = 0; j < 3; ++j) {
                        p = ((Uint32)p * 255) / A;
                    }
                }
#else
                Uint8 A = p;
                if (A) {
                    for (j = 1; j < 4; ++j) {
                        p = (p * 255) / A;
                    }
                }
#endif /* ENDIAN */
                p += 4;
            }
        }
    }
    
    if (color_space)
    {
        CGColorSpaceRelease(color_space);
    }
    
    return surface;
}

static int count=0;
void getAllImageFromAlbum()
{
//    ALAssetsLibrary *library;
//        
//    NSMutableArray *mutableArray;
//    
//    
//    mutableArray =[[NSMutableArray alloc]init];
//    
//    NSMutableArray* assetURLDictionaries = [[NSMutableArray alloc] init];
//    static SDL_Surface ** thumbs;
//    library = [[ALAssetsLibrary alloc] init];
//    
//    
//    
//    void (^assetEnumerator)( ALAsset *, NSUInteger, BOOL *) = ^(ALAsset *result, NSUInteger index, BOOL *stop) {
//        
//        if(result != nil) {
//            
//            if([[result valueForProperty:ALAssetPropertyType] isEqualToString:ALAssetTypePhoto]) {
//                
//                [assetURLDictionaries addObject:[result valueForProperty:ALAssetPropertyURLs]];
//                
//                
//                
//                NSURL *url= (NSURL*) [[result defaultRepresentation]url];
//                
//                
//                
//                [library assetForURL:url
//                 
//                         resultBlock:^(ALAsset *asset) {
//                             
//                             [mutableArray addObject:[UIImage imageWithCGImage:[[asset defaultRepresentation] fullScreenImage]]];
//                             
//                             
//                             
//                             if ([mutableArray count]==count)
//                                 
//                             {
//                                 thumbs = (SDL_Surface * *)malloc(sizeof(SDL_Surface *) * count);
//                                 for (int i = 0; i < count; i++) {
//                                     UIImage *image = [mutableArray objectAtIndex: i];
//                                     thumbs[i] = Create_SDL_Surface_From_CGImage(image.CGImage);
//                                 }
//                                 
////                                 int which = do_open1(thumbs, count);
//                                 
//                                 for (int i = 0; i < count; i++) {
//                                     if (which != i)
//                                        SDL_FreeSurface(thumbs[i]);
//                                 }
//                                 free(thumbs);
//   
//                                 
//                             }
//                         }
//                 
//                        failureBlock:^(NSError *error){ NSLog(@"operation was not successfull!"); } ];
//                
//                
//                
//            }
//            
//        }
//        
//    };
//
//    
//    
//    void (^ assetGroupEnumerator) ( ALAssetsGroup *, BOOL *)= ^(ALAssetsGroup *group, BOOL *stop) {
//        
//        if(group != nil) {
//            count=[group numberOfAssets];
//            
//            [group enumerateAssetsUsingBlock:assetEnumerator];
//                        
//            
//        }
//        
//    };
//    
//    
//    [library enumerateGroupsWithTypes:ALAssetsGroupAll
//     
//                           usingBlock:assetGroupEnumerator
//     
//                         failureBlock:^(NSError *error) {NSLog(@"There is an error");}];
//    
    
}
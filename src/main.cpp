#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>

#include "AudioDevice.hpp"
#include "SpectrumAnalyzer.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#define SAMPLE_RATE		48000
#define CHUNK_SIZE		512
#define MAX_BLOCK_SIZE	4096

#define THREAD_COUNT	1

#define FSTART	32.7032			//C1
#define FEND		16744.0384	//C10
#define BINS_PER_OCTAVE	3

#define WIN_WIDTH		1000
#define WIN_HEIGHT	600

struct X11_t {
	Display *dis;
	int screen;
	Window win;
	GC gc;
};

std::mutex displayMutex;

void x_init(X11_t* x11);
void x_close(X11_t* x11);

void x_drawSpectrum(X11_t*, std::shared_ptr<Spectrum>);

int main() {
	X11_t x11;

	//Initialize X11
	x_init(&x11);

	std::shared_ptr<AudioDevice> audioDevice(
		std::make_shared<AudioDevice>(AudioDevice::DEFAULT_DEVICE,
		SAMPLE_RATE, CHUNK_SIZE));

	SpectrumAnalyzer spectrumAnalyzer(audioDevice, FSTART, FEND,
		BINS_PER_OCTAVE, MAX_BLOCK_SIZE, THREAD_COUNT);

	spectrumAnalyzer.addListener([&x11](auto, auto left, auto) {
/*
		std::cout << "[Info] Dominant Frequency: "
			<< (int)left->getMaxFrequency() << "Hz\t\t"
			<< (int)right->getMaxFrequency() << "Hz" << std::endl;
*/
		std::cout << "[Info] Average energy: " << (int)left->getAverageEnergyDB() << "dB" << std::endl;

		x_drawSpectrum(&x11, left);
	});

	//Start stream
	audioDevice->startStream();

	//while(1) {
		//Wait 10 seconds
		//std::this_thread::sleep_for(std::chrono::seconds(10));
	//}

	//Wait for enter
	while(1) {
		static XEvent event;
		static char text[256];
		static KeySym key;

		XNextEvent(x11.dis, &event);

		if(event.type == KeyPress &&
			XLookupString(&event.xkey, text, 256, &key, 0) == 1) {

			if(text[0] == 'q')
				break;
		}
	}

	audioDevice->stopStream();

	//Close X11
	x_close(&x11);

	return 0;
}

void x_init(X11_t* x11) {
	unsigned long black, white;

	x11->dis = XOpenDisplay((char*)0);
	
	x11->screen = DefaultScreen(x11->dis);
	
	black = BlackPixel(x11->dis, x11->screen);
	white = WhitePixel(x11->dis, x11->screen);

	x11->win = XCreateSimpleWindow(x11->dis, DefaultRootWindow(x11->dis),
		0, 0, WIN_WIDTH, WIN_HEIGHT, 5, white, black);

	XSetStandardProperties(x11->dis, x11->win, "Spectrum Analyzer Display",
		"Spectrum Analyzer", None, NULL, 0, NULL);

	XSelectInput(x11->dis, x11->win,
		ExposureMask | ButtonPressMask | KeyPressMask);

	x11->gc = XCreateGC(x11->dis, x11->win, 0, 0);

	XSetBackground(x11->dis, x11->gc, black);
	XSetForeground(x11->dis, x11->gc, white);

	XClearWindow(x11->dis, x11->win);
	XMapRaised(x11->dis, x11->win);
}

void x_close(X11_t* x11) {
	XFreeGC(x11->dis, x11->gc);
	XDestroyWindow(x11->dis, x11->win);
	XCloseDisplay(x11->dis);
}

void x_drawSpectrum(X11_t* x11, std::shared_ptr<Spectrum> spectrum) {
	if(!displayMutex.try_lock())
		return;
	
	static std::unique_ptr<Spectrum> prevSpec;

	unsigned int width = WIN_WIDTH, height = WIN_HEIGHT, border = 10,
		maxBarHeight = height - 4*border, maxWidth = width - 4*border;
	double dbMin = -60., dbMax = 0.;

/*
	XGetGeometry(x11->dis, x11->win, &w, &i, &i, &width, &height,
		&u, &u);
*/
	auto binBegin = spectrum->begin();
	auto binEnd = spectrum->end();

	int binCount = binEnd - binBegin;

	if(!prevSpec) {
		prevSpec = std::make_unique<Spectrum>(*spectrum.get());
	}

	//Clear screen
	XClearWindow(x11->dis, x11->win);

	XDrawRectangle(x11->dis, x11->win, x11->gc, border, border, width-2*border,
		height - 2*border);
/*
	double avg = spectrum->getAverageEnergyDB() - dbMin;
	if(avg < 0)
		avg = 0;
*/
	for(int i = 0; i < binCount; ++i) {
		//double db = (binBegin + i)->getEnergyDB();
		double curDB = (spectrum->begin()+i)->getEnergyDB(),
			db = (prevSpec->begin()+i)->getEnergyDB();

			

		if(curDB > db) {
			db = curDB;
		}
		else {
			db = std::max(curDB, db - 0.75);
		}


		(prevSpec->begin()+i)->setEnergyDB(db);

		if(db < dbMin)
			db = dbMin;
		if(db > dbMax)
			db = dbMax;

		int barHeight = maxBarHeight * (db - dbMin) / (dbMax - dbMin),
			barWidth = maxWidth / binCount / 2;

		if(barHeight < 1)
			barHeight = 1;

		int x = i * maxWidth / binCount + 2*border,
			y = height - 2*border - barHeight;

		XFillRectangle(x11->dis, x11->win, x11->gc, x, y, barWidth, barHeight);
	}

	//Draw average line
	double avg = (spectrum->getAverageEnergyDB() - dbMin);
	double avgY = maxBarHeight * avg / (dbMax - dbMin);
	avgY = height -2*border - avgY;
	if(avgY < 1)
		avgY = 1;
	XDrawLine(x11->dis, x11->win, x11->gc, 2*border, avgY, width - 3*border, avgY);

	XFlush(x11->dis);

	//Delay
	//std::this_thread::sleep_for(std::chrono::milliseconds(75));

	displayMutex.unlock();
}

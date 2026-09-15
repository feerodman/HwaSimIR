#include <QApplication>
#include <QLabel>
#include <QPixmap>
#include <QScreen>
#include <cassert>
#include <iostream>
#include "../HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/VideoPixelFit.h"
int main(int argc,char**argv){
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc,argv);QLabel label;label.setScaledContents(false);label.setAlignment(Qt::AlignCenter);
    // Alternating physical pixels detect an accidental 800 -> logical 400 resize.
    QImage source(800,800,QImage::Format_RGB32);
    for(int y=0;y<800;++y)for(int x=0;x<800;++x)source.setPixel(x,y,(x+y)%2?qRgb(255,255,255):qRgb(0,0,0));
    const double dpr=2;const auto size=HwaVideoFit::fit(source.width(),source.height(),1200,1000);
    QPixmap p=QPixmap::fromImage(source);p.setDevicePixelRatio(dpr);label.resize(size.width/dpr,size.height/dpr);label.setPixmap(p);
    assert(label.size()==QSize(400,400));assert(label.pixmap()->size()==QSize(800,800));assert(label.pixmap()->toImage()==source);
    // Resolution switch, then constrained area: no source/recording mutation.
    for(const auto& dimensions:{QSize(640,480),QSize(1024,1024),QSize(1280,1024),QSize(800,800)}){
        QImage input(dimensions,QImage::Format_RGB32);input.fill(Qt::gray);
        const auto fitted=HwaVideoFit::fit(input.width(),input.height(),1200,1000);
        QPixmap view=QPixmap::fromImage(input.scaled(fitted.width,fitted.height,Qt::IgnoreAspectRatio,Qt::SmoothTransformation));
        view.setDevicePixelRatio(dpr);label.setPixmap(view);
        assert(view.width()<=input.width()&&view.height()<=input.height());assert(input.size()==dimensions);
    }
    std::cout<<"Qt5 pixmap: 800 physical pixels retained at DPR2 / 400 logical; alternating pixel equality PASS\nresolution switch and source immutability PASS\n";
}

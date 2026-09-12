// Existing Weather alpha silhouettes become world-fixed artistic density volumes.
// These PNGs are 2-D appearances, not measured slices or physical densities.
static PTA_uchar BuildP6CloudDensity(const P6GameConfig& config,int index,int size){
    const std::string path=config.root+"/"+config.shapeTextures[index%4];
    PNMImage image;
    if(!image.read(Filename::from_os_specific(path))||!image.has_alpha())
        throw std::runtime_error("P6 cloud shape missing alpha: "+path);
    auto sample=[&](double u,double v,bool alpha){
        if(u<0||u>1||v<0||v>1)return 0.0;
        double x=u*(image.get_x_size()-1),y=(1-v)*(image.get_y_size()-1);
        int ix=int(x),iy=int(y),jx=std::min(ix+1,image.get_x_size()-1),jy=std::min(iy+1,image.get_y_size()-1);
        double a=alpha?image.get_alpha(ix,iy):image.get_gray(ix,iy),b=alpha?image.get_alpha(jx,iy):image.get_gray(jx,iy);
        double c=alpha?image.get_alpha(ix,jy):image.get_gray(ix,jy),d=alpha?image.get_alpha(jx,jy):image.get_gray(jx,jy);
        return (a+(b-a)*(x-ix))*(1-(y-iy))+(c+(d-c)*(x-ix))*(y-iy);
    };
    PTA_uchar voxels=PTA_uchar::empty_array(size*size*size*4);
    for(int z=0;z<size;++z)for(int y=0;y<size;++y)for(int x=0;x<size;++x){
        double px=(x+.5)/size*2-1,py=(y+.5)/size*2-1,pz=(z+.5)/size*2-1;
        // Depth lobes/warp keep a finite 3-D body while retaining the asset edge.
        double u=.5+px*.57+.055*std::sin(py*4+index)*py;
        double v=.5+pz*.60+.035*std::cos(py*5+index)*py;
        double alpha=sample(u,v,true),shade=sample(u,v,false);
        double thickness=.28+.27*alpha;
        double dy=py/thickness;
        double depth=std::max(0.0,1-dy*dy);
        double boundary=std::max(0.0,std::min(1.0,(.98-px*px-py*py-pz*pz)/.12));
        double structure=.78+.22*std::sin(px*11+py*8+index)*std::cos(pz*9-py*7);
        double density=std::min(1.0,alpha*depth*boundary*structure*1.7);
        int k=((z*size+y)*size+x)*4;
        voxels[k]=static_cast<unsigned char>(density*255+.5);
        voxels[k+1]=static_cast<unsigned char>(shade*255+.5);voxels[k+2]=0;voxels[k+3]=255;
    }
    std::cout<<"[P6CloudAsset] template="<<index<<" source="<<path<<" sourceSize="<<image.get_x_size()<<"x"<<image.get_y_size()
        <<" alpha=1 densitySize="<<size<<" format=RGBA8 density=R appearance=G reconstruction=artistic_depth_lobes physicalDensity=0"<<std::endl;
    return voxels;
}

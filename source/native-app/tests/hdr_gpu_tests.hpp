// Included after the host GPU fixture; exercises production HDR shaders.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace {
struct HdrFixture {
    GLuint texture = 0;
    hui::gfx::Canvas target;
    hui::gfx::Renderer renderer;
    sx::HdrCompositor hdr;
    int w, h;
    HdrFixture(int width,int source_h,int output_h,const std::vector<uint32_t>& source) : w(width),h(output_h) {
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB10_A2,w,source_h,0,GL_RGBA,GL_UNSIGNED_INT_2_10_10_10_REV,source.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        require(target.create(w,h,1)&&renderer.init(),"HDR fixture allocation failed");
        std::string error; require(hdr.prepare(w,h,&error),error.c_str());
    }
    ~HdrFixture() {
        // The app keeps its renderer alive; isolated fixtures must unbind
        // their programs before member destruction and the next upload.
        glUseProgram(0); glBindVertexArray(0); glBindFramebuffer(GL_FRAMEBUFFER,0);
        glDeleteTextures(1,&texture);
    }
    void draw(const hui::gfx::DrawList& list,VideoRect rect,bool bgr=false) {
        renderer.begin(); renderer.draw(list); std::string error;
        require(hdr.present(renderer,texture,rect,target.framebuffer(),&error,bgr),error.c_str());
        glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer());
    }
    uint32_t pixel(int x,int y) {
        uint32_t result; glReadPixels(x,y,1,1,GL_BGRA,GL_UNSIGNED_BYTE,&result); return result;
    }
};
void hdr_gpu_tests() {
    test("HDR accepted BGR format reverses packed red/blue bits only",[] {
        const uint32_t word=uint32_t(101) | (uint32_t(502)<<10) | (uint32_t(907)<<20) | 0xc0000000u;
        HdrFixture fixture(2,2,2,std::vector<uint32_t>(4,word)); hui::gfx::DrawList list;
        fixture.draw(list,{0,0,2,2}); const uint32_t rgb=fixture.pixel(0,0);
        fixture.draw(list,{0,0,2,2},true); const uint32_t bgr=fixture.pixel(0,0);
        require((rgb&1023)==(bgr>>20&1023) && (rgb>>20&1023)==(bgr&1023),"BGR red/blue packing mismatch");
        require((rgb>>10&1023)==(bgr>>10&1023),"BGR changed green levels");
    });
    test("HDR10 bypass retains 877 PQ levels and rejects incompatible pictures",[] {
        Source s(877,2,true); s.fill(64,512,512); VideoFrame f;
        f.colors=YuvColors::Hdr10; f.hdr10_compatible=true;
        for (int row=0;row<2;++row) for (int x=0;x<877;++x) {
            const uint16_t word=uint16_t(64+x);
            std::memcpy(s.y.data()+row*s.stride+x*2,&word,2);
        }
        require(pack_video_yuv(s.view(),f)&&f.picture().hdr10_compatible,"HDR source flags lost");
        sx::VideoSurface surface; std::string error;
        require(surface.upload(f.picture(),&error,true),error.c_str());
        glBindTexture(GL_TEXTURE_2D,surface.texture());
        std::vector<float> values(877*2*4);
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,values.data());
        for (int x=0;x<877;++x) {
            require(std::abs(values[x*4]-float(x)/876)<1.00001f/1023,"PQ was tone mapped or reduced to 8-bit");
            if (x) require(values[x*4]>values[(x-1)*4],"PQ levels collapsed");
        }
        f.hdr10_compatible=false;
        require(!surface.upload(f.picture(),&error,true),"unknown gamut or DV profile 5 accepted as HDR10");
        f.hdr10_compatible=true; f.colors=YuvColors::Bt709;
        require(!surface.upload(f.picture(),&error,true),"SDR accepted as PQ");
    });
    test("final BGRA transport retains 1024 RGB codes with correct channel order",[] {
        const int w=1024,h=2; std::vector<uint32_t> source(w*h);
        for(int y=0;y<h;++y) for(int x=0;x<w;++x)
            source[y*w+x]=uint32_t(x)|(uint32_t(1023-x)<<10)|(uint32_t((x*37)&1023)<<20)|0xc0000000u;
        HdrFixture f(w,h,h,source); hui::gfx::DrawList list;
        f.draw(list,{0,0,float(w),float(h)});
        std::vector<uint32_t> row(w); glReadPixels(0,0,w,1,GL_BGRA,GL_UNSIGNED_BYTE,row.data());
        for(int x=0;x<w;++x) {
            const uint32_t expected=source[x]; // VideoOut RGB: red occupies the low ten bits
            require(row[x]==expected,"HDR output truncated, dithered or swapped a channel");
        }
        require(glGetError()==GL_NO_ERROR,"packed HDR readback failed");
    });
    test("HDR UI uses 100-nit white and blends shadows in linear light",[] {
        const int w=256,h=128;
        const auto pq=[](double x) { double p=std::pow(x,0.1593017578125); return std::pow((0.8359375+18.8515625*p)/(1+18.6875*p),78.84375); };
        const auto linear=[](double x) { double p=std::pow(x,1/78.84375); return std::pow(std::max(p-0.8359375,0.0)/(18.8515625-18.6875*p),1/0.1593017578125); };
        const uint32_t v=uint32_t(std::lround(pq(0.1)*1023));
        std::vector<uint32_t> source(w*h,v|(v<<10)|(v<<20)|0xc0000000u);
        HdrFixture f(w,h,h,source); hui::gfx::DrawList list;
        list.rounded_rect({0,0,960,1080},0,hui::gfx::Color::rgb(0xffffff));
        list.rounded_rect({960,0,960,1080},0,hui::gfx::Color::rgb(0x000000,0.5f));
        f.draw(list,{0,0,float(w),float(h)});
        const uint32_t white=f.pixel(w/4,h/2),shadow=f.pixel(w*3/4,h/2);
        for(int shift : {0,10,20}) {
            require(std::abs(int((white>>shift)&1023)-int(std::lround(pq(0.01)*1023)))<=1,"SDR white became HDR peak white");
            const int expected=int(std::lround(pq(linear(double(v)/1023)*(1-128.0/255))*1023));
            require(std::abs(int((shadow>>shift)&1023)-expected)<=2,"shadow blended in PQ instead of linear light");
        }
    });
    test("cropped 4K HDR keeps one-pixel stripes, orientation and black bars",[] {
        const int w=3840,h=1600,out_h=2160; std::vector<uint32_t> source(size_t(w)*h);
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            const uint32_t r=x%2?1023:0,g=y==0?1023:0,b=y==h-1?1023:0;
            source[size_t(y)*w+x]=r|(g<<10)|(b<<20)|0xc0000000u;
        }
        HdrFixture f(w,h,out_h,source); hui::gfx::DrawList list;
        f.draw(list,video_fit(w,h,1,float(w),float(out_h)));
        std::vector<uint32_t> row(w); glReadPixels(0,out_h/2,w,1,GL_BGRA,GL_UNSIGNED_BYTE,row.data());
        for(int x=0;x<w;++x)
            require(row[x]==(x%2?0xc00003ffu:0xc0000000u),"4K stripes were reduced or interpolated");
        require(f.pixel(0,1879)==0xc00ffc00u&&f.pixel(0,280)==0xfff00000u&&f.pixel(0,279)==0xc0000000u,
                "HDR crop orientation or black bars changed");
    });
}
}

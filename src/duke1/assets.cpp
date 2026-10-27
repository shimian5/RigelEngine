#include "assets.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <map>
namespace duke1 {
  static void need(bool b,const char*m){
    if(!b)throw std::runtime_error(m);
  }
  static uint16_t word(const std::vector<uint8_t>&b,size_t p){
    need(p+2<=b.size(),"truncated word");
    return b[p]|(uint16_t(b[p+1])<<8);
  }
  std::vector<uint8_t> read_file(const std::filesystem::path&r,const std::string&n){
    std::filesystem::path p(n);
    need(!p.is_absolute()&&p.parent_path().empty()&&n.find(':')==n.npos,"unsafe path");
    auto q=r;
    for(const auto&c:p){
      need(std::filesystem::is_directory(q),"missing directory");
      auto w=c.string();
      std::filesystem::path f;
      int m=0;
      for(auto&e:std::filesystem::directory_iterator(q)){
        auto a=e.path().filename().string();
        if(a.size()==w.size()&&std::equal(a.begin(),a.end(),w.begin(),[](char x,char y){
          return std::tolower((unsigned char)x)==std::tolower((unsigned char)y);
        }
        )){
          f=e.path();
          m++;
        }
      }
      need(m==1,m?"case-insensitive filename collision":"file not found");
      need(!std::filesystem::is_symlink(f),"symlink asset path rejected");
      q=f;
    }
    need(std::filesystem::is_regular_file(q),"not a file");
    std::ifstream in(q,std::ios::binary);
    return {
      std::istreambuf_iterator<char>(in),{
      }
    }
    ;
  }
  std::vector<uint8_t> Tile::rgba()const{
    static const uint8_t P[16][3]={
      {
        0,0,0
      }
      ,{
        0,0,170
      }
      ,{
        0,170,0
      }
      ,{
        0,170,170
      }
      ,{
        170,0,0
      }
      ,{
        170,0,170
      }
      ,{
        170,85,0
      }
      ,{
        170,170,170
      }
      ,{
        85,85,85
      }
      ,{
        85,85,255
      }
      ,{
        85,255,85
      }
      ,{
        85,255,255
      }
      ,{
        255,85,85
      }
      ,{
        255,85,255
      }
      ,{
        255,255,85
      }
      ,{
        255,255,255
      }
    }
    ;
    need(packed.size()==size_t(width_bytes)*height*5,"incomplete tile");
    std::vector<uint8_t>o;
    for(size_t i=0;i<packed.size();i+=5)for(int b=7;b>=0;b--){
      unsigned c=((packed[i+1]>>b)&1)|(((packed[i+2]>>b)&1)<<1)|(((packed[i+3]>>b)&1)<<2)|(((packed[i+4]>>b)&1)<<3);
      o.insert(o.end(),{
        P[c][0],P[c][1],P[c][2],uint8_t((packed[i]>>b&1)?255:0)
      }
      );
    }
    return o;
  }
  Tileset parse_tiles(const std::vector<uint8_t>&b){
    need(b.size()>=3,"missing graphics header");
    unsigned n=b[0],w=b[1],h=b[2];
    need(n&&w&&h,"zero header requires backdrop interpretation");
    size_t z=size_t(w)*h*5,e=3+size_t(n)*z;
    need(b.size()>=e,"incomplete tiles");
    Tileset t;
    t.header={
      b[0],b[1],b[2]
    }
    ;
    for(size_t i=0;i<n;i++)t.tiles.push_back({
      w,h,{
        b.begin()+3+i*z,b.begin()+3+(i+1)*z
      }
    }
    );
    t.trailing={
      b.begin()+e,b.end()
    }
    ;
    return t;
  }
  Tileset parse_backdrop(const std::vector<uint8_t>&b){
    need(b.size()>=3&&!b[0]&&!b[1]&&!b[2],"unexpected backdrop header");
    need(b.size()>=20803,"incomplete 130-tile backdrop");
    Tileset t;
    t.header={
      0,0,0
    }
    ;
    for(size_t i=0;i<130;i++)t.tiles.push_back({
      2,16,{
        b.begin()+3+i*160,b.begin()+3+(i+1)*160
      }
    }
    );
    t.trailing={
      b.begin()+20803,b.end()
    }
    ;
    return t;
  }
  BackdropPreview parse_backdrop_compat(const std::vector<uint8_t>&b,const std::string&label){
    need(b.size()>=3&&!b[0]&&!b[1]&&!b[2],"unexpected backdrop header");
    BackdropPreview r;
    r.image={
      208,160,std::vector<uint8_t>(208*160*4)
    }
    ;
    size_t n;
    if(b.size()>=20803)n=parse_backdrop(b).tiles.size();
    else{
      n=std::min<size_t>(130,(b.size()-3)/160);
      r.ignored_partial_bytes=(b.size()-3)%160;
      r.warning=label+" incomplete (shareware)";
    }
    r.complete_images=n;
    for(size_t i=0;i<n;i++){
      Tile t{
        2,16,{
          b.begin()+3+i*160,b.begin()+3+(i+1)*160
        }
      }
      ;
      auto x=t.rgba();
      for(int y=0;y<16;y++)for(int xx=0;xx<16;xx++){
        size_t a=(y*16+xx)*4,d=((i/13*16+y)*208+i%13*16+xx)*4;
        unsigned al=x[a+3];
        for(int c=0;c<3;c++)r.image.rgba[d+c]=(x[a+c]*al)/255;
        r.image.rgba[d+3]=al;
      }
    }
    return r;
  }
  Level parse_level(const std::vector<uint8_t>&b){
    need(b.size()==23040,"level must be exactly 128*90*2 bytes");
    Level l;
    for(size_t i=0;i<b.size();i+=2)l.cells.push_back(word(b,i));
    return l;
  }
  Screen parse_screen(const std::vector<uint8_t>&b){
    need(b.size()==32000,"screen must contain 4 * 8000 bytes");
    Screen s;
    s.indices.resize(64000);
    for(int y=0;y<200;y++)for(int x=0;x<320;x++){
      int c=0;
      for(int p=0;p<4;p++)c|=((b[p*8000+y*40+x/8]>>(7-x%8))&1)<<p;
      s.indices[y*320+x]=c;
    }
    return s;
  }
  SoundBank parse_sounds(const std::vector<uint8_t>&b){
    need(b.size()>=16&&b[0]=='S'&&b[1]=='N'&&b[2]=='D'&&!b[3],"missing SND signature");
    SoundBank r;
    r.declared_size=word(b,4);
    r.count=word(b,6);
    r.unknown=word(b,8);
    size_t z=r.declared_size,n=r.count;
    need(z>=16&&z<=b.size()&&16+n*16<=z,"bad sound bank extent");
    if(z!=b.size())r.warnings.push_back("declared size differs from physical size");
    struct E{
      uint16_t o;
      uint8_t p,q;
      std::string s;
    }
    ;
    std::vector<E>es;
    for(size_t i=0;i<n;i++){
      size_t a=16+i*16;
      E e{
        word(b,a),b[a+2],b[a+3],{
        }
      }
      ;
      for(int j=0;j<12&&b[a+4+j];j++)e.s+=char(b[a+4+j]);
      es.push_back(e);
    }
    for(size_t i=0;i<n;i++){
      size_t o=es[i].o,l=i+1<n?es[i+1].o:b.size();
      need(!(o&1)&&o>=16+n*16&&o<l&&l<=b.size(),"bad sound offset");
      Sound s;
      s.offset=o;
      s.priority=es[i].p;
      s.rate=es[i].q;
      s.name=es[i].s;
      size_t p=o;
      while(p+2<=l){
        auto v=word(b,p);
        p+=2;
        if(v==65535){
          s.terminated=true;
          break;
        }
        s.divisors.push_back(v);
      }
      if(!s.terminated){
        need(s.name=="__UnNamed__"&&std::all_of(s.divisors.begin(),s.divisors.end(),[](auto v){
          return !v;
        }
        ),"unterminated sound");
        r.warnings.push_back("zero-filled placeholder without terminator");
      }
      if(p>z)r.warnings.push_back("sound extends beyond declared size");
      r.sounds.push_back(std::move(s));
    }
    return r;
  }
  static void scene_blit(std::vector<uint8_t>&d,int dx,int dy,int dw,const std::vector<uint8_t>&src,int sw,int sh){
    for(int y=0;y<sh;y++)for(int x=0;x<sw;x++){
      size_t a=(size_t(y)*sw+x)*4,b=(size_t(dy+y)*dw+dx+x)*4;
      unsigned al=src[a+3];
      for(int c=0;c<3;c++)d[b+c]=(src[a+c]*al+d[b+c]*(255-al))/255;
      d[b+3]=uint8_t(al+d[b+3]*(255-al)/255);
    }
  }
  Scene render_scene(const std::filesystem::path&root,const std::string&level_name,bool compat,bool flat,std::optional<std::array<std::string,2>> context){
    static const std::map<std::string,std::array<std::string,2>> pairs={
      {
        "WORLDAL1",{
          "DROP0.DN1","DROP3.DN1"
        }
      }
      ,{
        "WORLDAL3",{
          "DROP0.DN1","DROP3.DN1"
        }
      }
      ,{
        "WORLDAL4",{
          "DROP7.DN1","DROP3.DN1"
        }
      }
      ,{
        "WORLDAL5",{
          "DROP3.DN1","DROP5.DN1"
        }
      }
      ,{
        "WORLDAL6",{
          "DROP7.DN1","DROP7.DN1"
        }
      }
      ,{
        "WORLDAL7",{
          "DROP2.DN1","DROP2.DN1"
        }
      }
      ,{
        "WORLDAL8",{
          "DROP1.DN1","DROP2.DN1"
        }
      }
      ,{
        "WORLDAL9",{
          "DROP3.DN1","DROP3.DN1"
        }
      }
      ,{
        "WORLDALA",{
          "DROP5.DN1","DROP2.DN1"
        }
      }
      ,{
        "WORLDALB",{
          "DROP0.DN1","DROP11.DN1"
        }
      }
      ,{
        "WORLDALC",{
          "DROP13.DN1","DROP9.DN1"
        }
      }
    }
    ;
    std::string stem=std::filesystem::path(level_name).stem().string();
    std::transform(stem.begin(),stem.end(),stem.begin(),[](unsigned char c){
      return std::toupper(c);
    }
    );
    auto level=parse_level(read_file(root,level_name));
    Scene result;
    result.image={
      2048,1440,std::vector<uint8_t>(2048*1440*4)
    }
    ;
    for(size_t i=3;i<result.image.rgba.size();i+=4)result.image.rgba[i]=255;
    std::array<BackdropPreview,2>drops{
    }
    ;
    bool have=false;
    std::array<std::string,2> pair{
    }
    ;
    if(context){
      pair=*context;
      have=true;
    }
    else if(!flat){
      auto it=pairs.find(stem);
      if(it!=pairs.end()){
        pair=it->second;
        have=true;
      }
    }
    if(have&&!flat)for(int i=0;i<2;i++){
      auto bytes=read_file(root,pair[i]);
      if(compat)drops[i]=parse_backdrop_compat(bytes,pair[i].substr(0,pair[i].find('.')));
      else{
        auto set=parse_backdrop(bytes);
        drops[i].image={
          208,160,std::vector<uint8_t>(208*160*4)
        }
        ;
        for(int j=0;j<130;j++)scene_blit(drops[i].image.rgba,(j%13)*16,(j/13)*16,208,set.tiles[j].rgba(),16,16);
      }
    }
    if(!flat&&!have){
      result.warnings.push_back(stem=="WORLDAL2"?"Flat backdrop: standalone intermission is context-dependent":"Flat backdrop: map-to-pair relationship unverified");
    }
    for(auto&d:drops)if(!d.warning.empty())result.warnings.push_back(d.warning);
    std::array<std::vector<uint8_t>,8>bank;
    for(int solid=0;solid<2;solid++)for(int n=0;n<4;n++){
      auto set=parse_tiles(read_file(root,std::string(solid?"SOLID":"BACK")+std::to_string(n)+".DN1"));
      auto& dst=bank[solid*4+n];
      dst.resize(96*128*4);
      for(int j=0;j<48;j++)scene_blit(dst,(j%6)*16,(j/6)*16,96,set.tiles.at(j).rgba(),16,16);
    }
    for(size_t i=0;i<level.cells.size();i++){
      unsigned v=level.cells[i],x=i%128,y=i/128;
      if(v==0||v==0x20){
        if(have&&!flat){
          auto&d=drops[v==0?0:1].image;
          for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx++){
            size_t a=(size_t((y%10)*16+yy)*208+(x%13)*16+xx)*4,b=(size_t(y*16+yy)*2048+x*16+xx)*4;
            unsigned al=d.rgba[a+3];
            for(int c=0;c<3;c++)result.image.rgba[b+c]=(d.rgba[a+c]*al+result.image.rgba[b+c]*(255-al))/255;
          }
        }
        continue;
      }
      if(v<0x3000){
        if(v%32)continue;
        int group=v<0x1800?v/0x600:4+(v-0x1800)/0x600,index=(v%0x600)/32;
        auto&src=bank[group];
        for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx++){
          size_t a=(size_t((index/6)*16+yy)*96+(index%6)*16+xx)*4,b=(size_t(y*16+yy)*2048+x*16+xx)*4;
          unsigned al=src[a+3];
          for(int c=0;c<3;c++)result.image.rgba[b+c]=(src[a+c]*al+result.image.rgba[b+c]*(255-al))/255;
        }
      }
    }
    return result;
  }
}

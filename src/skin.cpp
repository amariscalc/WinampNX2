#include "skin.hpp"
#include <cstdio>
#include <cstring>
#include <string_view>

namespace {
struct Glyph {uint8_t advance; uint16_t rows[16];};
#include "font.inc"
using Color=uint32_t;
constexpr Color paper=0xc4c6c5, ink=0x14151c, dark=0x242432, lit=0xe4e3d7,
    green=0x69f044, dimGreen=0x3d9852, well=0x050b09, gold=0xe8bd55, muted=0xacabb6;
Color mix(Color a,Color b,int n,int d) {
    Color c=0;for(int s:{0,8,16}) c|=((((a>>s&255)*(d-n)+(b>>s&255)*n)/d)<<s);
    return c;
}
struct Canvas {
    uint32_t* px;
    void box(int x,int y,int w,int h,Color c) {
        int a=std::clamp(x,0,width),b=std::clamp(x+w,0,width);
        if(a>=b) return;
        for(int j=std::max(0,y);j<std::min(height,y+h);j++)
            std::fill(px+j*width+a,px+j*width+b,c|0xff000000);
    }
    void grad(int x,int y,int w,int h,Color a,Color b) {
        for(int i=0;i<h;i++) box(x,y+i,w,1,mix(a,b,i,std::max(1,h-1)));
    }
    void edge(int x,int y,int w,int h,Color hi,Color lo) {
        box(x,y,w,1,hi);box(x,y,1,h,hi);box(x,y+h-1,w,1,lo);box(x+w-1,y,1,h,lo);
    }
    void inset(int x,int y,int w,int h,Color fill=well) {
        box(x,y,w,h,fill);edge(x,y,w,h,0x101017,0xa8a7ab);
    }
    void panel(int x,int y,int w,int h) {
        box(x+2,y+3,w,h,0x070811);
        grad(x,y,w,h,0x444554,0x292b3a);edge(x,y,w,h,0xc1c0c6,0x090910);
        edge(x+1,y+1,w-2,h-2,0x757783,0x191a23);
        edge(x+3,y+3,w-6,h-6,0x20202e,0x585a68);
    }
    static unsigned glyph(std::string_view s,size_t& i) {
        unsigned c=(unsigned char)s[i++];
        if(c<128) return c<32?' ':c;
        unsigned n=c<224?1:c<240?2:3,code=c&((1u<<(6-n))-1);
        while(n-- && i<s.size() && ((unsigned char)s[i]&192)==128) code=code*64+((unsigned char)s[i++]&63);
        return code>=32 && code<=255?code:'?';
    }
    int measure(std::string_view s,const Glyph* font) {
        int w=0; for(size_t i=0;i<s.size();) w+=font[glyph(s,i)-32].advance;
        return w;
    }
    void text(int x,int y,std::string_view s,Color color,const Glyph* font=small,int limit=640) {
        int start=x;
        for(size_t i=0;i<s.size();) {
            const auto& g=font[glyph(s,i)-32]; if(x-start+g.advance>limit) break;
            for(int j=0;j<16;j++) if(g.rows[j]) for(int k=0;k<16;k++) if(g.rows[j] & (1u<<k)) box(x+k,y+j,1,1,color);
            x+=g.advance;
        }
    }
    void marquee(int x,int y,std::string_view value,int room,double elapsed) {
        int length=measure(value,bold);
        if(length<=room) {text(x,y,value,green,bold,room);return;}
        int offset=std::clamp(int(elapsed*22)%(length+52),0,length-room);
        for(size_t i=0;i<value.size();) {
            size_t begin=i;const auto& g=bold[glyph(value,i)-32];
            if(offset<=0 && -offset+g.advance<=room) text(x-offset,y,value.substr(begin,i-begin),green,bold,g.advance);
            offset-=g.advance;
        }
    }
    void center(int x,int y,int w,std::string_view s,Color c,const Glyph* f=small) {
        text(x+(w-measure(s,f))/2,y,s,c,f,w);
    }
    void title(int x,int y,int w,std::string_view name) {
        int mid=measure(name,bold)+16;
        grad(x,y,w,15,0x282937,0x171824);
        for(int j=0;j<5;j++) {
            int k=(w-mid)/2;
            for(int p=0;p<k;p++) {
                Color tint=mix(0x6a5335,0xf9d475,p,k);
                box(x+p,y+3+j*2,1,1,tint);box(x+w-p-1,y+3+j*2,1,1,tint);
            }
        }
        center(x,y,w,name,lit,bold);
    }
    void button(int x,int y,int w,int h,std::string_view name,bool active=false) {
        grad(x,y,w,h,active?0x444646:0xc4c4c2,active?0x888983:0x82848f);
        edge(x,y,w,h,active?0x181920:0xf5f1df,active?0xe1dfd6:0x171923);
        edge(x+1,y+1,w-2,h-2,active?0x45454b:0xd1d1cb,active?0xbbbab1:0x646571);
        center(x,y+(h-11)/2,w,name,ink,small);
    }
    void indicator(int x,int y,std::string_view label,bool on) {
        inset(x,y+4,6,5,on?green:0x244226); text(x+9,y,label,on?lit:muted);
    }
    void slider(int x,int y,int w,int val,Color tint) {
        inset(x,y+3,w,6,0x14151c);
        int n=(w-7)*std::clamp(val,0,100)/100;
        box(x+2,y+5,n,2,tint);
        grad(x+n,y,8,12,0xf3e0a4,0xb0873f);edge(x+n,y,8,12,0xffeabd,ink);
        box(x+n+3,y+3,1,6,0x70522e);
    }
    void digit(int x,int y,int num) {
        static constexpr int masks[]={0x3f,6,0x5b,0x4f,0x66,0x6d,0x7d,7,0x7f,0x6f};
        constexpr int seg[][4]={{3,0,12,3},{15,3,3,9},{15,15,3,9},{3,24,12,3},{0,15,3,9},{0,3,3,9},{3,12,12,3}};
        for(int i=0;i<7;i++) box(x+seg[i][0],y+seg[i][1],seg[i][2],seg[i][3],masks[num]&(1<<i)?green:0x102513);
    }
    void triangle(int x,int y,int size,bool left,Color c) {
        for(int i=0;i<size;i++) box(x+(left?size-1-i:i),y+i/2,1,size-(i/2)*2,c);
    }
    void transport(int x,int y,int type) {
        button(x,y,30,23,""); int cx=x+10,cy=y+6;
        if(type==1||type==4) triangle(cx,cy,10,false,ink);
        if(type==0) triangle(cx,cy,10,true,ink);
        if(type==0||type==4) box(cx+(type==0?-2:9),cy,2,10,ink);
        if(type==2) {box(cx,cy,3,10,ink);box(cx+6,cy,3,10,ink);}
        if(type==3) box(cx,cy+1,9,9,ink);
    }
};
std::string clockText(double seconds) {
    int n=std::max(0,int(seconds)); char s[24];std::snprintf(s,sizeof s,"%d:%02d",n/60,n%60);return s;
}
void mainDeck(Canvas& c,const Player& p) {
    c.panel(8,10,358,168);c.title(14,15,346,"W I N A M P N X 2");
    c.inset(19,37,146,78);
    int sec=int(p.position), mins=sec/60;
    if(p.audio) {
        if(p.paused) {c.box(26,49,2,9,green);c.box(30,49,2,9,green);}
        else c.triangle(25,49,8,false,green);
    } else c.box(26,49,7,7,dimGreen);
    c.digit(44,44,(mins/10)%10);c.digit(66,44,mins%10);
    c.box(89,50,3,3,green);c.box(89,61,3,3,green);
    c.digit(99,44,sec%60/10);c.digit(121,44,sec%10);
    for(int i=0;i<32;i++) {
        int v=int(p.bars[i]*12);
        for(int j=0;j<12;j++) c.box(26+i*4,107-j*2,3,1,j<v?(j>9?0xf0683d:j>6?0xd1cb46:0x53cd52):0x112313);
    }
    std::string name=p.current<0?"WinampNX2":p.queue[p.current].name;
    if(p.audio && !p.audio->title.empty()) name=p.audio->title;
    if(p.audio && !p.audio->artist.empty()) name=p.audio->artist+" - "+name;
    c.inset(174,37,181,28);
    c.marquee(180,43,name,169,p.position);
    c.inset(174,71,33,16);c.center(174,73,33,p.audio?std::to_string(p.audio->kbps):"---",green);
    c.text(210,73,"KBPS",paper);
    c.inset(234,71,34,16);
    char freq[24]="--";if(p.audio) std::snprintf(freq,sizeof freq,"%g",p.audio->sourceRate/1000.);
    c.center(234,73,34,freq,green);c.text(271,73,"KHZ",paper);
    c.indicator(296,73,p.audio && p.audio->channels==1?"MONO":"STEREO",bool(p.audio));
    c.text(175,91,"VOL",paper);c.slider(196,91,67,p.eq.volume,0x8aba59);
    c.text(273,91,"BAL",paper);c.slider(295,91,58,(p.eq.balance+100)/2,0x65a353);
    c.text(175,108,p.audio?upper(p.audio->format):"STOPPED",gold,small,93);
    c.text(264,108,p.audio && p.audio->bits>0?std::to_string(p.audio->bits)+" BIT":"",muted);
    c.slider(20,125,334,p.audio && p.audio->duration>0?int(100*p.position/p.audio->duration):0,gold);
    for(int i=0;i<5;i++) c.transport(20+i*30,146,i);
    c.button(183,146,56,23,"SHUFFLE",p.shuffle);c.button(241,146,49,23,"REPEAT",p.repeat);
    c.button(298,146,56,23,"OPEN SD",p.view==View::Files);
}
void equalizer(Canvas& c,const Player& p) {
    c.panel(8,184,358,137);c.title(14,189,346,"GRAPHIC EQUALIZER");
    c.button(20,211,41,17,p.eq.on?"ON":"OFF",p.eq.on);
    c.button(65,211,43,17,"RESET");
    c.text(121,213,"PREAMP",muted);c.text(166,213,std::to_string(p.eq.preamp)+" dB",gold);
    int sel=p.band==0?p.eq.preamp:p.eq.gain[p.band-1];
    std::string status=p.view==View::Eq?(p.band?std::to_string(Equalizer::hz[p.band-1])+" Hz":"PREAMP")+"  "+std::to_string(sel)+" dB":p.eq.on?"EQ ENABLED":"FLAT / BYPASS";
    c.inset(232,210,121,19);c.center(232,214,121,status,p.eq.on?green:dimGreen);
    c.text(19,235,"+12",muted);c.text(24,261,"0",muted);c.text(19,286,"-12",muted);
    constexpr const char* names[]={"PRE","60","170","310","600","1K","3K","6K","12K","14K","16K"};
    for(int i=0;i<11;i++) {
        int x=i?76+(i-1)*29:47;
        int gain=i?p.eq.gain[i-1]:p.eq.preamp;
        c.inset(x-4,236,9,59,0x14151a);
        for(int j=0;j<24;j++) c.box(x-1,239+j*2,3,1,mix(0xd8663e,0x70b66a,j,23));
        int y=265-gain*2;
        c.grad(x-6,y-3,13,7,0xe6e5dc,0x9999a1);c.edge(x-6,y-3,13,7,lit,ink);
        c.box(x-4,y,9,1,0x615841);
        if(p.view==View::Eq && p.band==i) c.edge(x-9,233,19,64,gold,gold);
        c.center(x-11,300,23,names[i],i==0?gold:paper);
    }
}
void listing(Canvas& c,const Player& p) {
    bool browse=p.view==View::Files;
    auto& items=browse?p.files:p.queue;
    int active=browse?p.fileRow:p.selected,top=p.topRow();
    c.panel(374,10,258,311);c.title(380,15,246,browse?"SD FILE BROWSER":"PLAYLIST EDITOR");
    c.inset(383,38,240,24,0x14171c);
    c.text(390,44,browse?p.folder:p.queueName,browse?gold:paper,small,185);
    c.text(586,44,std::to_string(items.size()),dimGreen,small,30);
    c.inset(383,65,240,192);
    if(items.empty()) {
        c.text(393,86,browse?"No supported files in this folder":"Your playlist is empty",muted,body,220);
        c.text(393,108,browse?"B  Parent folder":"X  Browse SD",green);
    }
    for(int row=0;row<Player::visible && row+top<int(items.size());row++) {
        int i=row+top, y=69+row*20;
        auto& e=items[i];bool playing=!browse && i==p.current;
        if(i==active) c.box(387,y,226,19,0x252a71);
        Color color=i==active?0xf2f1dc:green;
        char num[16];std::snprintf(num,sizeof num,"%02d",i+1);
        if(playing) c.triangle(391,y+6,7,false,green);
        else c.text(390,y+4,e.directory?">":num,i==active?0xc3c4df:dimGreen);
        bool add=browse && e.track();
        c.text(409,y+1,e.name,color,body,add?146:160);
        std::string suffix=e.directory?"DIR":e.ext;
        c.text((add?588:608)-c.measure(suffix,small),y+4,suffix,i==active?0xd0cfdc:dimGreen);
        if(add) c.button(593,y,20,18,"+");
    }
    c.inset(615,70,5,181,0x171e19);
    int sh=std::max(9,180*std::min(Player::visible,int(items.size()))/std::max(1,int(items.size())));
    int pos=items.size()>Player::visible?(180-sh)*top/(int(items.size())-Player::visible):0;
    c.box(616,71+pos,3,sh,0x636a65);
    c.inset(383,263,240,20);
    std::string state=p.audio?(p.paused?"PAUSED":"PLAYING"):"STOPPED";
    c.text(390,268,p.notice.empty()?state:p.notice,p.notice.empty()?green:gold,small,p.notice.empty()?95:225);
    if(p.notice.empty()) {
        std::string times=clockText(p.position)+" / "+(p.audio?clockText(p.audio->duration):"--:--");
        c.text(616-c.measure(times,small),268,times,green);
    }
    c.button(383,291,62,20,"PLAYLIST",!browse);c.button(448,291,47,20,"FILES",browse);
    c.button(498,291,55,20,"MIX",p.menu);
    c.button(556,291,67,20,browse?"OPEN":"PLAY");
    if(p.menu) {
        c.panel(383,36,240,221);c.title(389,42,228,"MIX MENU");
        for(int row=0;row<Player::ActionCount;row++) {
            int y=66+row*29;
            bool focus=row==p.menuRow;
            c.box(392,y,221,25,focus?0x252a71:well);
            c.text(401,y+4,p.menuLabel(static_cast<Player::MixAction>(row)),focus?lit:green,body,203);
        }
        c.text(392,216,"Draft: "+std::to_string(p.draft.size())+" tracks",muted);
        c.button(544,226,68,20,"CLOSE");
    }
}
}
void Skin::draw(const Player& p) {
    Canvas c{pixels.data()};
    c.grad(0,0,width,height,0x151721,0x0b0d14);
    for(int y=0;y<height;y+=4) for(int x=y%8;x<width;x+=8)c.box(x,y,1,1,0x232433);
    mainDeck(c,p);equalizer(c,p);listing(c,p);
    c.box(9,332,622,1,0x343541);
    std::string guide=p.view==View::Eq?"D-PAD  Band / gain      A  EQ on/off      Y  Playlist":
        p.view==View::Files?"D-PAD  Select      A  Open      B  Parent      X  Playlist":
        "A  Play / pause     X  Files     Y  EQ     L/R  Track";
    if(p.menu) guide="D-PAD  Select      A  Choose      B  Close";
    c.text(13,339,guide,0xc1c1ca);
    c.text(466,339,"WinampNX2 0.2.0 / by amariscalc",gold,small);
}
void Skin::touch(Player& p,const Input& e) {
    int x=e.x,y=e.y;bool tap=e.key==Key::Tap;
    if(p.menu) {
        if(!tap) return;
        if(x>=392 && x<613 && y>=66 && y<66+Player::ActionCount*29) {
            p.mixAction(static_cast<Player::MixAction>((y-66)/29));
        } else p.menu=false;
        return;
    }
    if(x>=19 && x<=355 && y>=121 && y<=141 && p.audio) {
        p.seek((x-20)/334.*p.audio->duration);return;
    }
    if(y>=88 && y<=106 && x>=195 && x<=266) {p.eq.volume=std::clamp((x-196)*100/59,0,100);return;}
    if(y>=88 && y<=106 && x>=294 && x<=355) {p.eq.balance=std::clamp((x-295)*200/50-100,-100,100);return;}
    if(y>=234 && y<=296 && x>=37 && x<=348) {
        p.view=View::Eq;p.band=std::clamp(int(std::lround((x-47)/29.)),0,10);
        p.setGain(int(std::lround((265-y)/2.)));return;
    }
    if(!tap) return;
    if(y>=145 && y<=170 && x>=20 && x<170) {
        switch((x-20)/30) {
            case 0:p.step(-1);break;
            case 1:if(!p.audio)p.play(p.selected);else if(p.paused)p.toggle();break;
            case 2:p.toggle();break;case 3:p.stop();break;case 4:p.step(1);break;
        }
    } else if(y>=145 && y<=170 && x>=183 && x<=239) p.shuffle=!p.shuffle;
    else if(y>=145 && y<=170 && x>=241 && x<=290) p.repeat=!p.repeat;
    else if(y>=145 && y<=170 && x>=298 && x<=355) p.input(Key::Browse);
    else if(y>=210 && y<=229 && x>=20 && x<=61) p.toggleEq();
    else if(y>=210 && y<=229 && x>=65 && x<=108) {p.eq.preamp=0;p.eq.gain.fill(0);p.eq.rebuild();}
    else if(x>=386 && x<=613 && y>=69 && y<249) {
        int index=p.topRow()+(y-69)/20;
        if(p.view==View::Files) {if(index<int(p.files.size())) {p.fileRow=index;if(x>=593 && p.files[index].track()) p.mixAction(Player::Add);else p.chooseFile();}}
        else if(index<int(p.queue.size())) {p.view=View::Playlist;p.selected=index;p.enter();}
    } else if(x>=614 && x<=623 && y>=69 && y<=253) {
        bool browse=p.view==View::Files;int n=browse?p.files.size():p.queue.size();
        (browse?p.fileRow:p.selected)=std::clamp((y-69)*n/185,0,std::max(0,n-1));
    } else if(y>=290 && y<=312 && x>=383 && x<=445) p.view=View::Playlist;
    else if(y>=290 && y<=312 && x>=448 && x<=495) p.view=View::Files;
    else if(y>=290 && y<=312 && x>=498 && x<=553) p.openMenu();
    else if(y>=290 && y<=312 && x>=556 && x<=623) p.enter();
}

#include <stdio.h> // nanomysql, a tiny MySQL client
#include <stdarg.h>
#include <stdlib.h>
#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // on windows, cl /EHsc obfuscated.cpp
#include <io.h>
#include <winsock2.h>
#pragma comment(linker, "/defaultlib:ws2_32.lib")
void ZZ(){WSADATA t;if(WSAStartup(WINSOCK_VERSION,&t)){printf("failed to initialize WinSock2");exit(1);}}
#else
#include <sys/socket.h> // on unix, cc -lstdc++ obfuscated.cpp
#include <netdb.h>
#include <errno.h>
typedef unsigned char BYTE;typedef unsigned int DWORD;
#endif
#include <string.h>
#include <string>
#include <vector>
#define LL(value,bits) (((value)<<(bits))|((value)>>(32-(bits)))) // and let the brick begin
using namespace std;void die(const char*m,...){printf("*** error: ");va_list ap;va_start(ap,m);vprintf(m,ap);va_end(ap
);printf("\n");exit(1);}struct g{int h,p;BYTE*k,*l,*m,n[8192],*o;vector<string>r,s;string t;g (int L){h=L;k=l=m=new B\
YTE[1<<24];o=n;}void u(int M,const char*N){if(M>(1<<24))die("packet too big while reading %s\n",N);l=k;m=k+M;if(::recv
(h,(char*)k,M,0)!=M)die("recv failed while reading %s: %s",N,strerror(errno));}BYTE x(){if(l++<m)return l[-1];return 0
;}int v(){int v=x();switch(v){case 251:return 0;case 252:v=x();v+=x()<<8;return v;case 253:v=x();v+=x()<<8;v+=x()<<16;
return v;case 254:die("16M+ packets not supported");}return v;}void w(string &s){s="";int M=v();if(M&&l+M<=m)s.append(
(char*)l,M);l+=M;}virtual~g(){delete[]k;}DWORD y(){int r=x();r+=x()<<8;r+=x()<<16;return r+(x()<<24);}bool z(){return
l>m;}char*A(){return (char*)l;}int B(){return l<m?*l:-1;}void C(int n){l+=n;}void D(int P){if(o+P>n+sizeof(n))die("ne\
t write buffer overflow");}void E(BYTE uValue){D(1);*o++=uValue;}void F(DWORD v){D(4);for(int i=0;i<4;i++,v>>=8)*o++=
BYTE(v);}void G(const void*O,int M){D(M);memcpy(o,O,M);o+=M;}void H(){int M=o-n;if(::send(h,(char*)n,M,0)!=M)die("sen\
d failed: %s",strerror(errno));o=n;}int I(){u(4,"packet header");int M=y()&0xffffff;u(M,"packet b1");if(B()==255){t="\
mysql error: ";t.append(A()+9,M-9);return -1;}return B();}bool J(const char*q){int M=strlen(q);F((0<<24)+M+1);E(3);G(q
,M);H();r.resize(0);if((p=I())<0)return 0;r.resize(p);if(p==0)return 1;for(int i=0;i<p;i++){if(I()<0)return 0;for(int
j=0;j<4;j++)C(v());w(r[i]);C(v()+14);}if(I()!=254)die("unexpected packet type %d after fields packets",B());return 1;}
bool K(){if(p<=0)return 0;int i=I();if(i<0||i==254)return 0;s.resize(p);for(int i=0;i<p;i++)w(s[i]);return 1;}};struct
f{DWORD Q[5],a1[2];BYTE R[64];void T(const BYTE buf[64]){DWORD a=Q[0],b=Q[1],c=Q[2],d=Q[3],e=Q[4],S[16];memset(S,0,si\
zeof(S));for(int i=0;i<64;i++)S[i>>2]+=buf[i]<<((3-(i&3))*8);for(int i=0;i<80;i++){if(i>=16)S[i&15]=LL(S[(i+13)&15]^S[
(i+8)&15]^S[(i+2)&15]^S[i&15],1);if(i<20)e+=((b&(c^d))^d)+0x5A827999;else if(i<40)e+=(b^c^d)+0x6ED9EBA1;else if(i<60)e
+=(((b|c)&d)|(b&c))+0x8F1BBCDC;else e+=(b^c^d)+0xCA62C1D6;e+=S[i&15]+LL(a,5);DWORD t=e;e=d;d=c;c=LL(b,30);b=a;a=t;}Q[0
]+=a;Q[1]+=b;Q[2]+=c;Q[3]+=d;Q[4]+=e;}f &U(){const DWORD X[5]={0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0}
;memcpy(Q,X,sizeof(Q));a1[0]=a1[1]=0;return*this;}f &V(const BYTE*b1,int len){int i,j=(a1[0]>>3)&63;a1[0]+=(len<<3);if
(a1[0]<(DWORD)(len<<3))a1[1]++;a1[1]+=(len>>29);if((j+len)>63){i=64-j;memcpy(&R[j],b1,i);T(R);for(;i+63<len;i+=64)T(b1
+i);j=0;}else i=0;memcpy(&R[j],&b1[i],len-i);return*this;}void W(BYTE Y[20]){BYTE Z[8];for(int i=0;i<8;i++)Z[i]=(BYTE)
((a1[(i>=4)?0:1]>>((3-(i&3))*8))&255);V((BYTE*)"\200",1);while((a1[0]&504)!=448)V((BYTE*)"\0",1);V(Z,8);for(int i=0;i<
20;i++)Y[i]=(BYTE)((Q[i>>2]>>((3-(i&3))*8))&255);}};int main(int o1,const char**p1){const char*d1="localhost",*e1="ro\
ot",*i1="";int j1=3306;for(int i=1;i+1<o1;i+=2){if(!strcmp(p1[i],"-h"))d1=p1[i+1];else if(!strcmp(p1[i],"-u"))e1=p1[i+
1];else if(!strcmp(p1[i],"-p"))i1=p1[i+1];else if(!strcmp(p1[i],"-P"))j1=atoi(p1[i+1]);else die("unknown switch %s\nu\
sage: nanomysql [-h host] [-P port] [-u user] [-p password]",p1[i]);}ZZ();hostent*q1=gethostbyname(d1);if(!q1||q1->h_\
addrtype!=AF_INET)die("no AF_INET address found for %s",d1);sockaddr_in sin;sin.sin_family=AF_INET;sin.sin_port=htons(
j1);memcpy(&sin.sin_addr,*(in_addr**)q1->h_addr_list,sizeof(in_addr));int L=socket(AF_INET,SOCK_STREAM,0);if(L<0||con\
nect(L,(sockaddr*)&sin,sizeof(sin))<0)die("connection failed: %s",strerror(errno));g db(L);db.I();string f1;BYTE g1[21
],h1;db.x();do{f1.push_back(db.x());}while(f1.end()[-1]);db.y();for(int i=0;i<8;i++)g1[i]=db.x();db.C(3);h1=db.x();db.
C(15);for(int i=0;i<13;i++)g1[i+8]=db.x();if(db.z())die("failed to parse mysql handshacket packet");db.F((1<<24)+34+s\
trlen(e1)+(strlen(i1)?21:1));db.F(0x4003F7CFUL);db.F((1<<24)-1);db.E(h1);for(int i=0;i<23;i++)db.E(0);db.G(e1,strlen(
e1)+1);if(!i1||!*i1){db.E(0);}else {BYTE k1[20],l1[20],m1[20];f n1;n1.U().V((BYTE*)i1,strlen(i1)).W(k1);n1.U().V(k1,20
).W(l1);n1.U().V(g1,20).V(l1,20).W(m1);db.E(20);for(int i=0;i<20;i++)db.E(m1[i]^k1[i]);}db.E(0);db.H();if(db.I()<0)die
("auth failed: %s",db.t.c_str());printf("connected to mysql %s\n\n",f1.c_str());char q[4096];for(;;){printf("nanomysq\
l> ");fflush(stdout);if(!fgets(q,sizeof(q),stdin)||!strcmp(q,"quit\n")||!strcmp(q,"exit\n")){printf("bye\n\n");break;}
if(!db.J(q)){printf("error: %s\n\n",db.t.c_str());continue;}int n=0;for(size_t i=0;i<db.r.size();i++)printf("%s%s",i?
", ":"",db.r[i].c_str());if(db.r.size())printf("\n---\n");while(db.K()){for(size_t i=0;i<db.s.size();i++)printf("%s%s"
,i?", ":"",db.s[i].c_str());printf("\n");n++;}printf("---\nok, %d row(s)\n\n",n);}}
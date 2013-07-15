// nanomysql, a tiny MySQL client

#ifdef _MSC_VER
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <io.h>
	#include <winsock2.h>
	#pragma comment(linker, "/defaultlib:ws2_32.lib")
#else
	#include <sys/socket.h>
	#include <netdb.h>
	#include <errno.h>
	typedef unsigned char BYTE;
	typedef unsigned int DWORD;
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
using namespace std;

#define MAX_PACKET	16777216 // bytes
#define SHA1_SIZE	20

void die ( const char * msg, ... )
{
	printf ( "*** error: " );
	va_list ap;
	va_start ( ap, msg );
	vprintf ( msg, ap );
	va_end ( ap );
	printf ( "\n" );
	exit ( 1 );
}

struct MysqlDriver_t
{
	int		m_iSock, m_iCols;
	BYTE	*m_pReadBuf, *m_pReadCur, *m_pReadMax, m_dWriteBuf[8192], *m_pWriteBuf;
	vector<string>	m_dFields, m_dRow;
	string			m_sError;

	MysqlDriver_t ( int iSock )
	{ 
		m_iSock = iSock;
		m_pReadBuf = m_pReadCur = m_pReadMax = new BYTE [ MAX_PACKET ];
		m_pWriteBuf = m_dWriteBuf;
	}

	void ReadFrom ( int iLen, const char * sWhat )
	{
		if ( iLen>MAX_PACKET )
			die ( "packet too big while reading %s\n", sWhat );
		m_pReadCur = m_pReadBuf;
		m_pReadMax = m_pReadBuf + iLen;
		if ( recv ( m_iSock, (char*)m_pReadBuf, iLen, 0 )!=iLen )
			die ( "recv failed while reading %s: %s", sWhat, strerror(errno) ); // strerror fails on Windows, but who cares
	}

	BYTE GetByte ()
	{
		if ( m_pReadCur++ < m_pReadMax )
			return m_pReadCur[-1];
		return 0;
	}

	int GetMysqlInt ()
	{
		int v = GetByte(); // 0 on error
		switch ( v )
		{
			case 251: return 0; // NULL column
			case 252: v = GetByte(); v += GetByte()<<8; return v; // 2-byte length
			case 253: v = GetByte(); v += GetByte()<<8; v += GetByte()<<16; return v; // 3-byte length
			case 254: die ( "16M+ packets not supported" ); // 8-byte length
		}
		return v;
	}

	void GetMysqlString ( string & s )
	{
		s = "";
		int iLen = GetMysqlInt();
		if ( iLen && m_pReadCur+iLen <= m_pReadMax )
			s.append ( (char*)m_pReadCur, iLen );
		m_pReadCur += iLen;
	}

	virtual			~MysqlDriver_t()	{ delete [] m_pReadBuf; }
	DWORD			GetDword()			{ int r = GetByte(); r += GetByte()<<8; r += GetByte()<<16; return r + ( GetByte()<<24 ); }
	bool			GetReadError()		{ return m_pReadCur > m_pReadMax; }
	char *			GetCur() 			{ return (char*)m_pReadCur; }
	int				PeekByte()			{ return m_pReadCur < m_pReadMax ? *m_pReadCur : -1; }
	void			SkipBytes ( int n )	{ m_pReadCur += n; }

	void SendEnsure ( int iBytes )
	{
		if ( m_pWriteBuf+iBytes > m_dWriteBuf+sizeof(m_dWriteBuf) )
			die ( "net write buffer overflow" );
	}

	void SendByte ( BYTE uValue )
	{
		SendEnsure(1);
		*m_pWriteBuf++ = uValue;
	}

	void SendDword ( DWORD v )
	{
		SendEnsure(4);
		for ( int i=0; i<4; i++, v>>=8 )
			*m_pWriteBuf++ = BYTE(v);
	}

	void SendBytes ( const void * pBuf, int iLen )
	{
		SendEnsure(iLen);
		memcpy ( m_pWriteBuf, pBuf, iLen );
		m_pWriteBuf += iLen;
	}

	void Flush()
	{
		int iLen = m_pWriteBuf - m_dWriteBuf;
		if ( send ( m_iSock, (char*)m_dWriteBuf, iLen, 0 )!=iLen )
			die ( "send failed: %s", strerror(errno) );
		m_pWriteBuf = m_dWriteBuf;
	}

	int ReadPacket()
	{
		ReadFrom ( 4, "packet header" );
		int iLen = GetDword() & 0xffffff; // byte len[3], byte packet_no
		ReadFrom ( iLen, "packet data" );
		if ( PeekByte()==255 )
		{
			m_sError = "mysql error: ";
			m_sError.append ( GetCur()+9, iLen-9 ); // 9 bytes == byte type, word errcode, byte marker, byte sqlstate[5]
			return -1;
		}
		return PeekByte();
	}

	bool Query ( const char * q )
	{
		int iLen = strlen(q); // send that query
		SendDword ( (0<<24) + iLen + 1 ); // 0 is packet id
		SendByte ( 3 ); // COM_QUERY
		SendBytes ( q, iLen );
		Flush();

		m_dFields.resize ( 0 );
		if ( ( m_iCols = ReadPacket() )<0 ) // fetch response packet
			return false;

		m_dFields.resize ( m_iCols );
		if ( m_iCols==0 ) // 0 means OK but no further packets
			return true;
		for ( int i=0; i<m_iCols; i++ ) // read and parse field packets
		{
			if ( ReadPacket()<0 )
				return false;
			for ( int j=0; j<4; j++ )
				SkipBytes ( GetMysqlInt() ); // skip 4 strings (catalog, db, table, orgtable)
			GetMysqlString ( m_dFields[i] ); // field_name
			SkipBytes ( GetMysqlInt()+14 ); // string orig_name, byte filler, word charset, dword len, byte type, word flags, byte decimals, word filler2
		}
		if ( ReadPacket()!=254 ) // eof packet expected after fields
			die ( "unexpected packet type %d after fields packets", PeekByte() );
		return true;
	}

	bool FetchRow()
	{
		if ( m_iCols<=0 )
			return false;
		int i = ReadPacket();
		if ( i<0 || i==254 ) // mysql error or eof
			return false;
		m_dRow.resize ( m_iCols );
		for ( int i=0; i<m_iCols; i++ )
			GetMysqlString ( m_dRow[i] );
		return true;
	}
};

struct SHA1_t
{
	DWORD state[5], count[2];
	BYTE buffer[64];

	void Transform ( const BYTE buf[64] )
	{
		DWORD a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], block[16];
		memset ( block, 0, sizeof(block) ); // initial conversion to big-endian units
		for ( int i=0; i<64; i++ )
			block[i>>2] += buf[i] << ((3-(i & 3))*8);
		for ( int i=0; i<80; i++ ) // do hashing rounds
		{
			#define _LROT(value,bits) ( ( (value) << (bits) ) | ( (value) >> ( 32-(bits) ) ) )
			if ( i>=16 )
				block[i&15] = _LROT ( block[(i+13)&15] ^ block[(i+8)&15] ^ block[(i+2)&15] ^ block[i&15], 1 );

			if ( i<20 )			e += ((b&(c^d))^d) + 0x5A827999;
			else if ( i<40 )	e += (b^c^d) + 0x6ED9EBA1;
			else if ( i<60 )	e += (((b|c) & d) | (b & c)) + 0x8F1BBCDC;
			else				e += (b^c^d) + 0xCA62C1D6;

			e += block[i&15] + _LROT ( a, 5 );
			DWORD t = e; e = d; d = c; c = _LROT ( b, 30 ); b = a; a = t;
		}
		state[0] += a; // save state
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;
	}

	SHA1_t & Init()
	{
		const DWORD dInit[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
		memcpy ( state, dInit, sizeof(state) );
		count[0] = count[1] = 0;
		return *this;
	}

	SHA1_t & Update ( const BYTE * data, int len )
	{
		int i, j = ( count[0]>>3 ) & 63;
		count[0] += ( len<<3 );
		if ( count[0] < (DWORD)( len<<3 ) )
			count[1]++;
		count[1] += (len >> 29);
		if ( ( j+len )>63 )
		{
			i = 64-j;
			memcpy ( &buffer[j], data, i );
			Transform ( buffer );
			for ( ; i+63<len; i+=64 )
				Transform ( data+i );
			j = 0;
		} else
			i = 0;
		memcpy ( &buffer[j], &data[i], len-i );
		return *this;
	}

	void Final ( BYTE digest[SHA1_SIZE] )
	{
		BYTE finalcount[8];
		for ( int i=0; i<8; i++ )
			finalcount[i] = (BYTE)( ( count[ ( i>=4 ) ? 0 : 1 ] >> ( (3-(i & 3))*8 ) ) & 255 ); // endian independent
		Update ( (BYTE*)"\200", 1 ); // add padding
		while ( ( count[0] & 504 )!=448 )
			Update ( (BYTE*)"\0", 1 );
		Update ( finalcount, 8 ); // should cause a SHA1_Transform()
		for ( int i=0; i<SHA1_SIZE; i++ )
			digest[i] = (BYTE)((state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
	}
};

int main ( int argc, const char ** argv)
{
	const char *sHost = "localhost", *sUser = "root", *sPass = "";
	int iPort = 3306;
	for ( int i=1; i+1<argc; i+=2 )
	{
		if ( !strcmp ( argv[i], "-h" ) )		sHost = argv[i+1];
		else if ( !strcmp ( argv[i], "-u" ) )	sUser = argv[i+1];
		else if ( !strcmp ( argv[i], "-p" ) )	sPass = argv[i+1];
		else if ( !strcmp ( argv[i], "-P" ) )	iPort = atoi(argv[i+1]);
		else die ( "unknown switch %s\nusage: nanomysql [-h host] [-P port] [-u user] [-p password]", argv[i] );
	}

#ifdef _MSC_VER
	WSADATA tWSAData;
	if ( WSAStartup ( WINSOCK_VERSION, &tWSAData ) )
		die ( "failed to initialize WinSock2" );
#endif

	// resolve host, prepare socket
	hostent * pHost = gethostbyname ( sHost );
	if ( !pHost || pHost->h_addrtype!=AF_INET )
		die ( "no AF_INET address found for %s", sHost );

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons ( iPort );
	memcpy ( &sin.sin_addr, *(in_addr **)pHost->h_addr_list, sizeof(in_addr) );

	int iSock = socket ( AF_INET, SOCK_STREAM, 0 );
	if ( iSock<0 || connect ( iSock, (sockaddr*)&sin, sizeof(sin) )<0 )
		die ( "connection failed: %s", strerror(errno) );

	// get and parse handshake packet
	MysqlDriver_t db ( iSock );
	db.ReadPacket();

	string sVer;
	BYTE dScramble[21], uLang;

	db.GetByte(); // proto_version
	do { sVer.push_back ( db.GetByte() ); } while ( sVer.end()[-1] ); // server_version
	db.GetDword(); // thread_id
	for ( int i=0; i<8; i++ )
		dScramble[i] = db.GetByte();
	db.SkipBytes(3); // byte filler1, word caps_lo
	uLang = db.GetByte();
	db.SkipBytes(15); // word status, word caps_hi, byte scramble_len, byte filler2[10]
	for ( int i=0; i<13; i++ )
		dScramble[i+8] = db.GetByte();

	if ( db.GetReadError() )
		die ( "failed to parse mysql handshacket packet" );

	// send auth packet
	db.SendDword ( (1<<24) + 34 + strlen(sUser) + ( strlen(sPass) ? 21 : 1 ) ); // byte len[3], byte packet_no
	db.SendDword ( 0x4003F7CFUL ); // all CLIENT_xxx flags but SSL, COMPRESS, SSL_VERIFY_SERVER_CERT, NO_SCHEMA
	db.SendDword ( MAX_PACKET-1 ); // max_packet_size, 16 MB
	db.SendByte ( uLang );
	for ( int i=0; i<23; i++ )
		db.SendByte ( 0 ); // filler
	db.SendBytes ( sUser, strlen(sUser)+1 ); // including trailing zero
	if ( !sPass || !*sPass )
	{
		db.SendByte ( 0 ); // 0 password length = no password
	} else
	{
		BYTE dStage1[SHA1_SIZE], dStage2[SHA1_SIZE], dRes[SHA1_SIZE];
		SHA1_t sha;
		sha.Init().Update ( (BYTE*)sPass, strlen(sPass) ).Final ( dStage1 );
		sha.Init().Update ( dStage1, SHA1_SIZE ).Final ( dStage2 );
		sha.Init().Update ( dScramble, 20 ).Update ( dStage2, SHA1_SIZE ).Final ( dRes );
		db.SendByte ( SHA1_SIZE );
		for ( int i=0; i<SHA1_SIZE; i++ )
			db.SendByte ( dRes[i] ^ dStage1[i] );
	}
	db.SendByte ( 0 ); // just a trailing zero instead of a full DB name
	db.Flush();

	if ( db.ReadPacket()<0 )
		die ( "auth failed: %s", db.m_sError.c_str() );

	// action!
	printf ( "connected to mysql %s\n\n", sVer.c_str() );
	char q[4096];
	for ( ;; )
	{
		printf ( "nanomysql> " );
		fflush ( stdout );
		if ( !fgets ( q, sizeof(q), stdin ) || !strcmp ( q, "quit\n" ) || !strcmp ( q, "exit\n" ) )
		{
			printf ( "bye\n\n" );
			break;
		}
		if ( !db.Query(q) )
		{
			printf ( "error: %s\n\n", db.m_sError.c_str() );
			continue;
		}
		int n = 0;
		for ( size_t i=0; i<db.m_dFields.size(); i++ )
			printf ( "%s%s", i ? ", " : "", db.m_dFields[i].c_str() );
		if ( db.m_dFields.size() )
			printf ( "\n---\n" );
		while ( db.FetchRow() )
		{
			for ( size_t i=0; i<db.m_dRow.size(); i++ )
				printf ( "%s%s", i ? ", " : "", db.m_dRow[i].c_str() );
			printf ( "\n" );
			n++;
		}
		printf ( "---\nok, %d row(s)\n\n", n );
	}
}
@find = qw(SHA1_t MysqlDriver_t m_iSock m_pReadBuf m_pReadCur m_pReadMax m_dWriteBuf
m_pWriteBuf m_iCols m_dFields m_dRow m_sError ReadFrom GetMysqlInt GetMysqlString GetByte GetDword
GetReadError GetCur PeekByte SkipBytes SendEnsure SendByte SendDword SendBytes Flush ReadPacket Query
FetchRow iSock iLen sWhat pBuf iBytes state buffer block Transform Init Update Final
dInit digest finalcount count data iType sHost sUser sPass iPort pHost sVer dScramble uLang dStage1
dStage2 dRes sha argc argv);

$n = 0;
$ident = 8; # skip abcde, ij, q
$idents = "abcdeijqfghklmnoprstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
for $k (@find)
{
	$a = substr($idents, ($ident % length($idents)), 1);
	$b = int($ident++/length($idents));
	$b = "" if ($b==0);
	$replace[$n++] = "$a$b";
}

# $n = 0;
# for $k (@find) { print $k, "=>", $replace[$n++], "\n"; }

while (<>)
{
	chomp;
	s/\/\/.*?$//g;
	if ( /#\w+/ )
	{
		s/^\s+//g;
		print "$_\n";
		next;
	}

	s/(void|class|typedef|using|namespace|const|va_list|int|BYTE|explicit|DWORD|bool|new|return|struct|else|static|string|public|WSADATA|sockaddr_in|unsigned|short|MysqlDriver_t|SHA1_t|char|size_t|case)\s/\1\$/g;

	$o = "";
	$intext = 0;
	for ($i=0; $i<length($_); $i++)
	{
		$c = substr($_,$i,1);
		if ($c eq ' ' && $intext eq 1)
		{
			$o.='$'
		} else {
			if ( $c eq '"' ) { $intext = 1 - $intext; }
			$o .= $c;
		}
	}
	$_ = $o;

	s/[ \t]+//g;
	s/\$/ /g;
	s/\s+\*/\*/g;
	s/\bSHA1_SIZE\b/20/g;
	s/\bMAX_PACKET\b/\(1<<24\)/g;
	s/\btrue\b/1/g;
	s/\bfalse\b/0/g;
	next if (length($_)==0);

	$n = 0;
	for $k (@find) { $r = $replace[$n++]; s/\b$k\b/$r/g; }

	print;
	print " " if (/else$/);
#	print "\n";
}

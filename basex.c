#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <stdarg.h>
#include <signal.h>
const char *argp_program_version = "basex-1.0";
const char *argp_program_bug_address = "zoeurk@gmail.com";
static struct argp_option
options[] = { 	{"infile",'i', "FILE",0, "input file (- for stdin)", 0 },
		{"output", 'o', "FILE", 0, "output file (- for stdout)", 0},
		{"decode",'d', NULL ,0,"decode (default is to encode)",0},
		{"base", 'b', "BASE",0, "select base BIN,16,16min,32,32hex,64,64url",0},
		{"cwrap", 'w',"COLUMN_WRAP",0,"ajuste la longueur des colonnes",0},
		{"lwrap", 'W',"LINE_WRAP",0,"ajuste la longueur des lignes",0},
		{"ignore-gabage",'g',NULL,0, "ignore les carcateres hors base",0},
		{0}
	};
char *base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char *base64url = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
char *base32 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
char *base32hex = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
char *base16 = "0123456789ABCDEF";
char *base16min = "0123456789abcdef";
char *base2 = "01";
char pad = '=';
union do_it{
	void (*enc)(char *,char *,char *,...);
	void (*dec)(char *,char *);
}do_it;
struct basex{
	int indef,outdef,decode,___;
	char *alpha;
	char *pad;
	union do_it fait;
	char *input,*output;
}basex;
struct arguments{
	struct basex *def;
	int infile,outfile,decode, base, garbage, ___;
	size_t cwrap,lwrap;
	char *buffer;
	off_t buflen;
}arguments;
static struct arguments args = {NULL,0,STDOUT_FILENO,0,-1,0,0,0,0,NULL,0};
static struct basex *b;
static void tobase64(char *input, char *output,char *base, ...);
static void frombase64(char *input, char *output);
static void tobase32(char *input,char *output,char *base, ...);
static void frombase32(char *input,char *output);
static void tobase16(char *input,char *output,char *base,...);
static void frombase16(char *input,char *output);
static void tobase2(char *input, char *output, char *base, ...);
static void frombase2(char *input,char *output);
static void * fn_base64(char *alpha,int decode);
static void * fn_base32(char *alpha,int decode);
static void * fn_base16(char *alpha,int decode);
static void * fn_base2(char *alpha,int decode);
int squeeze_char(char buffer,char *nocharlist)
{	char *pcharlst;
	for(	pcharlst = nocharlist;
		*pcharlst != '\0';
		pcharlst++
	)if(buffer == *pcharlst)
		return 0;
	return 1;
}
ssize_t read_with_wrap(struct basex *b, int garbage,int fd)
{	ssize_t inc;
	char *pbuf = b->output;
	for(	inc = 0;
		inc < b->outdef && read(fd,pbuf,1);
		((garbage != 0 && (	squeeze_char(b->output[inc],b->alpha) == 0 
					|| (b->pad && b->output[inc] == *b->pad))
		) && (inc++,pbuf++,1))
		|| (garbage == 0 && (	squeeze_char(b->output[inc]," \n") != 0
					|| (b->pad && b->output[inc] == *b->pad)) && (pbuf++,inc++,1)
		)
	);
	return inc;
}
void write_with_wrap(struct basex *b, ssize_t cwrap, ssize_t lwrap, int fd)
{	static ssize_t _cwrap_ = 0, _lwrap_ = 1;
	static ssize_t inc;
	for( 	inc = 0;
		inc < b->outdef && errno == 0;
		lwrap != 0 && _lwrap_ == lwrap && _cwrap_ == cwrap && (write(fd,"\n",1),_lwrap_ = 1, _cwrap_ = 0),
		errno == 0 && _cwrap_ != 0 && _cwrap_ == cwrap && (write(fd," ",1),_cwrap_ = 0,_lwrap_++ ),
		errno == 0 && b->output[inc] != '\0' && write(fd,&b->output[inc],1),
		inc++,
		_cwrap_++
	);
	if(errno){
		perror("write()");
		exit(EXIT_FAILURE);
	}
}
ssize_t buf_unwrap(char *buffer,char *orig,ssize_t len)
{	char *pbuf, *pbuf_;
	ssize_t i,j;
	for(	pbuf = orig,
		pbuf_ = buffer,
		i = j = 0;
		*pbuf != '\0' && len >i;
		pbuf[0] != ' '	&& pbuf[0] != '\n'  && pbuf[0] != '\t'
				&&(	*pbuf_ = *pbuf,
					pbuf++,
					pbuf_++,i++
				),
		(pbuf[0] == ' ' || pbuf[0] == '\n') && (++j,pbuf++)

	);
	return j;
}
void buf_garbage(struct basex *b, int offset,char *buffer, off_t *buflen)
{	char *pbuf,*pbuf_;
	for(	pbuf = pbuf_ = buffer;
		*pbuf != '\0' && buflen[0]>0;
		(*pbuf != b->output[offset] && (*pbuf_ = *pbuf,pbuf_++,pbuf++,1))
			|| (buflen[0]>0 && (pbuf++,buflen[0]--))
		
	);
	memcpy(b->output,buffer,b->outdef);
}
int
decode_basex(char *output,int outdef,char *alpha,char *padchar)
{	char *pinput,*palpha;
	int i,j,k,l;
	for(	pinput = output,
		palpha = alpha,
		j = i = 0;
		*palpha == '\0' && ++j,
		i < outdef && !j;
		i++,
		pinput++
	){	if(j) return i;
		for(	palpha = alpha,
			k=0,j=0,l=0;
			padchar != NULL && *pinput == *padchar &&
				(*pinput = -1, l = 1, k = 0),
			*palpha == *pinput && 
				(*pinput = k, l = 1) ,
			*palpha != '\0' && !l;
			palpha++,
			k++
		);
	}
	if(j)return i;
	return 0;
}
static error_t
parse_opt
	(int key, char *arg, struct argp_state *state)
{	struct arguments *arguments = state->input;
	char *nobases[7] = {"BIN","16","16min","32","32hex","64","64url"};
	switch(key)
	{	case 'i': if(*arg == '-')
			  {	arguments->infile = STDIN_FILENO;
			  }
			  else
			  {	if((arguments->infile = open(arg,O_RDONLY))<0)
			  	{	fprintf(stderr,"I can not open %s\n",arg);
					exit(errno);
				}
  			 }
			  break;
		case 'o': if(*arg == '-')
			  {	arguments->outfile = STDOUT_FILENO;
			  }
			  else
			  {	if((arguments->outfile = open(arg,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP))<0)
			  	{	fprintf(stderr,"I can not open %s\n",arg);
					exit(errno);
				}

			  }
			  break;
		case 'g': arguments->garbage = 1;
			  break;
		case 'd': arguments->decode = 1;
			  break;
		case 'b': for( 	arguments->base = 0;
				arguments->base<7 && 
					strncasecmp(nobases[arguments->base],arg,strlen(arg))!=0;
				arguments->base++);;
			  if(arguments->base >= 7)
			  {	fprintf(stderr,"Unknow base: %s\n",arg);
			  	exit(EXIT_FAILURE);
			  }
			  break;
		case 'w': arguments->cwrap = (size_t)atoi(arg);break;
		case 'W': arguments->lwrap = (size_t)atoi(arg);break;
		default:if(arguments->buffer == NULL && arg!=NULL)
			{	arguments->buffer = arg;
				arguments->buflen = strlen(arg);
				if(arguments->buflen == 0)
					exit(0);
				return 0;
			}
			break;
	}
	return 0;
}
void end(int signum){
	if(b->input)free(b->input);
	if(args.infile)close(args.infile);
	if(args.outfile)close(args.outfile);
	exit(EXIT_SUCCESS);
	
}
void destroy(void *p,int *fdin,int *fdout)
{	if(p)free(p);
	if(*fdin)close(*fdin);
	if(*fdout)close(*fdout);
}

static struct argp argp = {options,parse_opt,NULL,NULL,NULL,NULL,NULL};
int main(int argc, char **argv)
{	
	ssize_t i;
	int j;
	argp_parse(&argp,argc,argv,0,0,&args);
	switch(args.base)
	{	case 0:args.def = fn_base2(base2,args.decode);break;
		case 1:args.def = fn_base16(base16,args.decode);break;
		case 2:args.def = fn_base16(base16min,args.decode);break;
		case 3:args.def = fn_base32(base32,args.decode);break;
		case 4:args.def = fn_base32(base32hex,args.decode);break;
		case 5:	case -1:
			args.def = fn_base64(base64,args.decode);break;
		case 6:args.def = fn_base64(base64url,args.decode);break;
	}
	b = args.def;
	if(signal(SIGINT,end) == SIG_ERR){
		perror("signal()");
		destroy(b->input,&args.infile,&args.outfile);
		exit(EXIT_FAILURE);
	}
	if(args.buffer == NULL)
	{	switch(b->decode)
		{
		case 0: while((i = read(args.infile,b->input,b->indef)))
			{	b->fait.enc(b->input,b->output,b->alpha,i);
				write_with_wrap(b,args.cwrap,args.lwrap,args.outfile);
				memset(b->output,'\0',b->outdef);
				memset(b->input,'\0',b->indef);

			}
			break;
		case 1: while((i=(read_with_wrap(b,args.garbage,args.infile))))
			{	if( (j=decode_basex(b->output,i,b->alpha,b->pad)))
				{	
					if(!args.garbage)
					{	fprintf(stderr,"\nerreur en entrée:<%c>\n",b->output[j-1]);
						destroy(b->input,&args.infile,&args.outfile);
						exit(EXIT_FAILURE);
					}
				}
				if(b->output[0] == -1 || b->output[1] == -1)
				{
					fprintf(stderr,"\nerreur en entrée.\n");
					destroy(b->input,&args.infile,&args.outfile);
					exit(EXIT_FAILURE);
				}
				b->fait.dec(b->output,b->input);
				if(i != b->outdef)
				{	fprintf(stderr,"\nerreur en enrée\n");
					destroy(b->input,&args.infile,&args.outfile);
					exit(EXIT_FAILURE);
				}
				if(write(args.outfile,b->input,b->indef) < 0){
					perror("write()");
					exit(EXIT_FAILURE);
				}
				memset(b->output,'\0',b->outdef);
				memset(b->input,'\0',b->indef);
			}
			break;
		}
	}
	else
	{	do{	switch(b->decode)
			{	case 0:	if(args.buflen>=b->indef)
					{	memcpy(b->input,args.buffer,b->indef);
						b->fait.enc(b->input,b->output,b->alpha,b->indef);
						args.buffer+=b->indef;
						args.buflen-=b->indef;
						write_with_wrap(b,args.cwrap,args.lwrap,args.outfile);
					}
					else
					{	memcpy(b->input,args.buffer,args.buflen);
						b->fait.enc(b->input,b->output,b->alpha,args.buflen);
						write_with_wrap(b,args.cwrap,args.lwrap,args.outfile);
						args.buflen = 0;
					}
				break;
				case 1:	if(args.buflen>=b->outdef)
					{	memset(b->input,'\0',b->indef);
						i = buf_unwrap(b->output,args.buffer,b->outdef);
						while((j = decode_basex(b->output,b->outdef,b->alpha,b->pad)))
						{	if(!args.garbage)
							{
								fprintf(stderr,"\nerreur en entrée:<%c>\n",b->output[j-1]);
								destroy(b->input,&args.infile,&args.outfile);
								exit(EXIT_FAILURE);
							}
							else
							{	buf_garbage(b, j-1,args.buffer,&args.buflen);
								if(args.buflen<b->outdef)
								{	destroy(b->input,&args.infile,&args.outfile);
									fprintf(stderr,"\nerreur en entrée.\n");
									exit(EXIT_FAILURE);
								}
							}
						}
						if(b->output[0] == -1 || b->output[1] == -1)
						{	destroy(b->input,&args.infile,&args.outfile);
							fprintf(stderr,"\nerreur en entrée.\n");
							exit(EXIT_FAILURE);
						}
						b->fait.dec(b->output,b->input);
						if(write(args.outfile,b->input,b->indef) < 0){
							perror("write()");
							exit(EXIT_FAILURE);
						}
						args.buffer += (b->outdef + i);
						args.buflen -= (b->outdef + i);
					}
					else
					{	printf("\nerreur en entree.\n");
						args.buflen = 0;
					}
					break;
			}
			memset(b->input,'\0',b->indef);
			memset(b->output,'\0',b->outdef);
		}while(args.buflen);
	}
	destroy(b->input,&args.infile,&args.outfile);
	return 0;
}
void
tobase64(char *input, char *output,char *base, ...)
{	va_list ap;
	int len;
	unsigned char *pinput = (unsigned char *)input;
	va_start(ap,base);
	len = va_arg(ap,int);
	output[0] = base[pinput[0]>>2];
	output[1] = base[((pinput[0]&3)<<4) | pinput[1]>>4];
	output[2] = (len<2)? '=' :base[((pinput[1]&15)<<2) | (pinput[2]>>6)];
	output[3] = (len<3)? '=' :base[(pinput[2]&63)];

}
void
frombase64(char *input, char *output)
{	unsigned char *in = (unsigned char *)input,*out = (unsigned char *)output;
	out[0] = (in[0]<<2) | (in[1]>>4);
	out[1] = (in[2] == 255)? 0 : (in[1]<<4) | (in[2]>>2);
	out[2] = (in[3] == 255)? 0 :( in[2]<<6) | in[3];
}
void
tobase32(char *input,char *output,char *base, ...)
{	va_list ap;
	int len;
	unsigned char *pinput = (unsigned char *)input;
	va_start(ap,base);
	len = va_arg(ap,int);
	output[0] = base[pinput[0]>>3];
	output[1] = base[((pinput[0]&7)<<2) | (pinput[1]>>6)];
	output[2] = (len<2) ? '=' : base[((pinput[1]&63)>>1)];
	output[3] = (len<2) ? '=' : base[((pinput[1]&1)<<4) | (pinput[2]>>4)];
	output[4] = (len<3) ? '=' : base[((pinput[2]&15)<<1) |  (pinput[3]>>7)];
	output[5] = (len<4) ? '=' : base[((pinput[3]&127)>>2)];
	output[6] = (len<4) ? '=' : base[((pinput[3]&3)<<3) | (pinput[4]>>5)];
	output[7] = (len<5) ? '=' : base[(pinput[4]&31)];
}
void frombase32(char *input,char *output)
{	unsigned char *in = (unsigned char *)input,*out =(unsigned char *)output;
	out[0] = (in[0]<<3) | (in[1] >>2);
	out[1] = (in[3] == 255) ? 0 : (in[1]<<6) | (in[2] << 1) | (in[3] >>4);
	out[2] = (in[5] == 255) ? 0 : (in[3]<<4) | (in[4]>>1);
	out[3] = (in[6] == 255) ? 0 :(in[4]<<7) | (in[5]<<2) | in[6]>>3;
	out[4] = (in[7] == 255) ? 0 : (in[6]<<5) | in[7];
}
void tobase16(char *input,char *output,char *base,...)
{	unsigned char *pinput = (unsigned char *)input;
	output[0] = base[pinput[0] >>4];
	output[1] = base[pinput[0]&15];

}
void frombase16(char *input,char *output)
{	
	output[0] = (input[0]<<4) | input[1];
}
void tobase2(char *input, char *output, char *base, ...)
{
	output[0] = base[(*input&128)>>7];
	output[1] = base[(*input&64)>>6];
	output[2] = base[(*input&32)>>5];
	output[3] = base[(*input&16)>>4];
	output[4] = base[(*input&8)>>3];
	output[5] = base[(*input&4)>>2];
	output[6] = base[(*input&2)>>1];
	output[7] = base[(*input&1)];
}
void frombase2(char *input,char *output)
{	unsigned char *pinput = (unsigned char *)input;
	output[0] = (pinput[0]<<7)| (pinput[1]<<6) | (pinput[2]<<5) | (pinput[3]<<4) | (pinput[4]<<3) | (pinput[5]<<2) | (pinput[6]<<1) | pinput[7];
}
void * fn_base64(char *alpha,int decode)
{	static struct basex b = {3,4,0,0,NULL,NULL,{NULL},NULL,NULL};
	if(decode){	b.fait.dec = &frombase64;
	}else{		b.fait.enc = &tobase64; }
	b.alpha = alpha;
	b.decode = decode;
	if(!(b.input = calloc((3+4),sizeof(char))))
	{	fprintf(stderr,"can not alloc memory");
		exit(errno);
	}
	b.pad = &pad;
	b.output = &b.input[3];
	return &b;
}
void * fn_base32(char *alpha,int decode)
{	static struct basex b = {5,8,0,0,NULL,NULL,{NULL},NULL,NULL};
	if(decode){	b.fait.dec = &frombase32;
	}else{		b.fait.enc = &tobase32;}
	b.decode = decode;
	b.alpha = alpha;
	if(!(b.input = calloc((5+8),sizeof(char))))
	{	fprintf(stderr,"can not alloc memory");
		exit(errno);
	}
	b.pad = &pad;
	b.output = &b.input[5];
	return &b;
}
void * fn_base16(char *alpha,int decode)
{	static struct basex b = {1,2,0,0,NULL,NULL,{NULL},NULL,NULL};
	if(decode){	b.fait.dec = &frombase16;
	}else{		b.fait.enc = &tobase16;}
	b.decode = decode;
	b.alpha = alpha;
	if (!(b.input = calloc((1+2),sizeof(char))))
	{	fprintf(stderr,"can not alloc memory");
		exit(errno);
	}
	b.output = &b.input[1];
	return &b;
}
void * fn_base2(char *alpha,int decode)
{	static struct basex b = {1,8,0,0,NULL,NULL,{NULL},NULL,NULL};
	if(decode){	b.fait.dec = &frombase2;
	}else{		b.fait.enc = &tobase2;}
	b.decode = decode;
	b.alpha = alpha;
	if (!(b.input = calloc((1+8),sizeof(char))))
	{	fprintf(stderr,"can not alloc memory");
		exit(errno);
	}
	b.output = &b.input[1];
	return &b;
}

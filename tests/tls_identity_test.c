#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../patches/aurora_tls_identity.h"
static char *read_cert(const char *path, size_t *size) {
    FILE *f=fopen(path,"rb");char *pem;long length;
    assert(f);assert(fseek(f,0,SEEK_END)==0);length=ftell(f);assert(length>0);
    rewind(f);pem=malloc((size_t)length);assert(pem);
    assert(fread(pem,1,(size_t)length,f)==(size_t)length);fclose(f);
    *size=(size_t)length;return pem;
}
#define MATCH(host,expected) assert(aurora_tls_identity_matches(pem,size,host,strlen(host))==(expected))
int main(int argc,char **argv) {
    size_t size;char *pem;
    assert(argc==5);
    pem=read_cert(argv[1],&size);
    MATCH("voice.omp.ru",1);MATCH("VOICE.OMP.RU",1);
    MATCH("deep.voice.omp.ru",0);MATCH("omp.ru",0);
    MATCH("voice.omp.ru.attacker.test",0);MATCH("voice.notomp.ru",0);
    MATCH("voiceomp.ru",0);MATCH("127.0.0.1",0);MATCH("",0);
    assert(!aurora_tls_identity_matches(pem,size,"voice.omp.ru\0.attacker",22));
    free(pem);
    pem=read_cert(argv[2],&size); /* CN matches; SAN deliberately does not. */
    MATCH("voice.omp.ru",0);MATCH("other.omp.ru",1);free(pem);
    pem=read_cert(argv[3],&size);
    MATCH("127.0.0.1",1);MATCH("127.0.0.2",0);MATCH("::1",1);free(pem);
    pem=read_cert(argv[4],&size);MATCH("voice.omp.ru",0);free(pem);
    assert(!aurora_tls_identity_matches("invalid",7,"voice.omp.ru",12));
    puts("TLS identity tests passed");return 0;
}

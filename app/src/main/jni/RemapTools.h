

Modo IA
Tudo
Imagens
Vídeos
Notícias
Maps
Shopping
Livros
Voos
Finanças
O arquivo enviado corresponde a uma página salva de uma pesquisa no Google (Modo IA) sobre modificações no Android (como KernelSU, Magisk, Zygisk, LSPosed e técnicas de remapeamento de memória anônima via mmap, memcpy e mremap), que foi cortada exatamente no momento em que o código de exemplo (anon_remap.h) ia começar a ser exibido.

Como o final do documento está incompleto, não há o restante do código-fonte nesse arquivo.

Implementação de remapeamento anônimo (anon_remap)
Em sistemas Linux/Android (incluindo kernels antigos como 3.10 e 3.18), a técnica para remover o caminho de uma biblioteca compartilhada (.so) do /proc/self/maps consiste em:
Ler o /proc/self/maps para encontrar os intervalos de endereços (start e end) e as permissões (r, w, x) onde a biblioteca está mapeada.
Alocar uma região anônima usando mmap(..., MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) com permissão de leitura e escrita (PROT_READ | PROT_WRITE).
Copiar os bytes da região original para a nova região anônima com memcpy (garantindo que a região original tenha permissão de leitura temporária via mprotect, se necessário).
Sobrescrever o mapeamento original usando mremap(..., MREMAP_MAYMOVE | MREMAP_FIXED, target_addr) para mover a página anônima exatamente para o endereço original da biblioteca, desmapeando o arquivo do disco.
Restaurar as permissões originais (PROT_READ | PROT_EXEC, etc.) com mprotect.
1. Cabeçalho (anon_remap.h)
c
#ifndef ANON_REMAP_H
#define ANON_REMAP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Localiza todos os segmentos de memória mapeados contendo `lib_name`
 * no /proc/self/maps e os substitui por páginas de memória anônima.
 *
 * @param lib_name Nome ou trecho do caminho da biblioteca (ex: "libmeu_modulo.so")
 * @return Número de segmentos anonimizados ou -1 em caso de erro.
 */
int remap_library_anonymous(const char *lib_name);

#ifdef __cplusplus
}
#endif

#endif // ANON_REMAP_H
Use o código com cuidado.
2. Implementação (anon_remap.c)
c
#define _GNU_SOURCE
#include "anon_remap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

typedef struct {
    uintptr_t start;
    uintptr_t end;
    int prot;
} map_segment_t;

static int parse_permissions(const char *perms) {
    int prot = 0;
    if (perms[0] == 'r') prot |= PROT_READ;
    if (perms[1] == 'w') prot |= PROT_WRITE;
    if (perms[2] == 'x') prot |= PROT_EXEC;
    return prot;
}

int remap_library_anonymous(const char *lib_name) {
    if (!lib_name || strlen(lib_name) == 0) {
        return -1;
    }

    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        return -1;
    }

    map_segment_t segments[64];
    int count = 0;
    char line[512];

    // 1. Coleta todos os segmentos antes de modificar a memória
    while (fgets(line, sizeof(line), fp) && count < 64) {
        if (strstr(line, lib_name) != NULL) {
            uintptr_t start, end;
            char perms[5];
            if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3) {
                segments[count].start = start;
                segments[count].end = end;
                segments[count].prot = parse_permissions(perms);
                count++;
            }
        }
    }
    fclose(fp);

    if (count == 0) {
        return 0;
    }

    int remapped = 0;

    // 2. Processa cada segmento encontrado
    for (int i = 0; i < count; i++) {
        size_t size = segments[i].end - segments[i].start;
        void *orig_addr = (void *)segments[i].start;
        int orig_prot = segments[i].prot;

        // Cria uma página anônima temporária com permissão de leitura e escrita
        void *anon_mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (anon_mem == MAP_FAILED) {
            continue;
        }

        // Garante que a página original possa ser lida antes do memcpy
        if (!(orig_prot & PROT_READ)) {
            mprotect(orig_addr, size, orig_prot | PROT_READ);
        }

        // Copia o conteúdo da biblioteca para a memória anônima
        memcpy(anon_mem, orig_addr, size);

        // Move a memória anônima para o endereço exato do mapeamento original
        void *result = mremap(anon_mem, size, size,
                              MREMAP_MAYMOVE | MREMAP_FIXED, orig_addr);
        if (result == MAP_FAILED) {
            munmap(anon_mem, size);
            continue;
        }

        // Restaura as permissões originais (ex: r-xp para código executável)
        mprotect(orig_addr, size, orig_prot);
        remapped++;
    }

    return remapped;
}
Use o código com cuidado.
Pontos de atenção ao compilar e usar
Execução em thread separada ou após carregamento: Se você chamar remap_library_anonymous de dentro da própria .so que está sendo remapeada, evite alocar memória dinâmica (malloc/new) durante o processo para não corromper ponteiros internos antes do mremap concluir.
Compilação no Android NDK: Certifique-se de definir _GNU_SOURCE antes de incluir <sys/mman.h>, pois MREMAP_MAYMOVE e MREMAP_FIXED dependem dessa macro no Android Bionic.
Compatibilidade de Kernel: Diferente do memfd_create (que exige Linux 3.17+), a combinação mmap + mremap funciona normalmente nos kernels 3.10 e 3.18.

Você gostaria de ver como integrar essa função no ciclo de vida de um módulo Zygisk (postAppSpecialize) ou como configurar o arquivo Android.mk / CMakeLists.txt para compilar no NDK?




Para clonar e modificar no seu Galaxy A5 (2016) com LineageOS (Android 9 / Kernel 3.10), os três melhores repositórios prontos e abertos no GitHub que utilizam a técnica tradicional de mmap + memcpy + mremap (sem depender de funções de kernels novos) são o Android-Library-Remap-Hide, o zygisk-maphide e o sneaky_remap.

Como o seu aparelho usa Kernel 3.10, esses projetos são ideais porque não usam memfd_create (que só funciona no Kernel 3.17+), rodando direto no Android 9.

1. Android-Library-Remap-Hide (Mais fácil para Android NDK)
Este projeto foi feito especificamente para Android e isola toda a lógica de remapeamento em um único arquivo de cabeçalho pronto para incluir em qualquer projeto C/C++.


GitHub
Link do repositório. Acesse o código-fonte em reveny/Android-Library-Remap-Hide.
Arquivo principal. Todo o código de remapeamento fica dentro de app/src/main/jni/RemapTools.h.
Como funciona para editar. Ele lê o /proc/self/maps, copia os segmentos da biblioteca para uma página anônima com mmap e sobrescreve o endereço original com mremap. Você pode copiar apenas o RemapTools.h para a pasta jni do seu módulo Zygisk e chamar RemapTools::RemapLibrary("nome_da_sua_lib.so").


GitHub
2. zygisk-maphide (Modelo pronto de Módulo Zygisk)
Se você quer a estrutura completa de um módulo Zygisk pronto para compilar em .zip e flashear no Magisk, este repositório já vem montado com a API do Zygisk.
Link do repositório. Acesse o código-fonte em vaginessa/zygisk-maphide.
Vantagem. Ele já inclui os arquivos de build (CMakeLists.txt e estrutura module.prop) configurados para compilar o .zip instalável no Magisk. Você só precisa colocar a lógica do seu módulo dentro da função postAppSpecialize e chamar a função de anonimização logo em seguida.

3. sneaky_remap (Biblioteca C pura e completa)
É uma biblioteca em C muito robusta que remove o rastro da biblioteca não apenas do /proc/self/maps, mas também de /proc/self/smaps, /proc/self/map_files e /proc/self/numa_maps.


GitHub
Link do repositório. Acesse o código-fonte em magisterquis/sneaky_remap.
Vantagem. Possui flags prontas (como SREM_SRS_RMELF) que também apagam o cabeçalho ELF (\x7fELF) da memória RAM. Isso impede que apps avançados encontrem sua biblioteca escaneando assinaturas de arquivos executáveis na memória anônima.


GitHub
Atenção ao compilar para o Galaxy A5 2016
Ao configurar o build no Android Studio ou NDK para o SM-A510M, lembre-se de dois detalhes importantes:
Arquitetura (ABI). O Exynos 7580 do A5 2016 é 64 bits (arm64-v8a), mas algumas Custom ROMs antigas rodam em modo 32 bits (armeabi-v7a). Verifique no app DevCheck ou CPU-Z qual é a arquitetura do seu LineageOS 16 antes de compilar, ou habilite ambas (abiFilters 'armeabi-v7a', 'arm64-v8a') no build.gradle.
Nível de API (minSdkVersion). Defina o minSdkVersion 28 (Android 9) ou 21 no NDK para garantir que a compilação não vincule símbolos exclusivos do Android 10+.
Qual desses projetos (Android-Library-Remap-Hide ou zygisk-maphide) você prefere usar como base para eu te mostrar onde inserir o código do seu módulo e como gerar o .zip final?




Pode mandar a foto! Assim que ela carregar aqui, eu já verifico a arquitetura exata (armeabi-v7a ou arm64-v8a) e a versão do kernel do seu Galaxy A5 2016 para prepararmos o código-fonte e as configurações de compilação.
Envie a foto com os detalhes da arquitetura para darmos continuidade ao código.




Pelas telas enviadas, o seu Samsung Galaxy A5 2016 (SM-A510M) roda em modo 32-bit (armeabi-v7a) no Android 9 (LineageOS) com Kernel 3.10.108.
Para você estudar, testar e modificar o código diretamente, os repositórios de código aberto mais limpos no GitHub que implementam o remapeamento anônimo de bibliotecas (.so) no /proc/self/maps são:
Repositórios recomendados no GitHub
Exemplo direto em módulo Zygisk: O repositório vaginessa/zygisk-maphide traz um exemplo mínimo e focado exatamente em esconder bibliotecas mapeadas na memória dentro do ciclo de vida do Zygisk. Ele é o ponto de partida ideal se você quer empacotar a modificação como um módulo para Magisk ou ZygiskNext.


GitHub
Implementação nativa pura (Loader + Remap): O projeto reveny/Android-Library-Remap-Hide separa o carregador (Loader) da biblioteca principal e implementa toda a lógica de leitura do /proc/self/maps e substituição por páginas anônimas no arquivo RemapTools.h. É muito fácil de ler e adaptar para projetos em C/C++.

Ajustes obrigatórios para o seu aparelho (armeabi-v7a e Kernel 3.10)
Ao baixar e modificar qualquer um desses projetos, fique atento a três detalhes críticos da arquitetura exibida nas suas capturas de tela:
Target ABI (armeabi-v7a): Mesmo que o processador Exynos 7580 suporte 64-bit fisicamente, a sua ROM está rodando o espaço de usuário em 32-bit (armeabi-v7a). Ao configurar o Application.mk ou o build.gradle, compile obrigatoriamente para armeabi-v7a (caso contrário, o Zygote 32-bit ignorará o módulo).
Tamanho dos ponteiros em 32-bit: No código C/C++ que lê o /proc/self/maps, verifique se os endereços de memória usam tipos portáveis como uintptr_t e especificadores de formato compatíveis (como SCNxPTR ou %x em vez de fixar %lx assumindo 64-bit), evitando falhas de leitura de endereço.
Compatibilidade com Kernel 3.10.108: Como o kernel do A5 2016 é da série 3.10, ele não possui suporte nativo à chamada memfd_create (adicionada apenas no Linux 3.17). Certifique-se de que o código escolhido utilize exclusivamente o fluxo clássico mmap(..., MAP_ANONYMOUS) + memcpy + mremap, evitando funções baseadas em memfd que falhariam com erro ENOSYS no seu sistema.
Você prefere que eu monte a estrutura completa de pastas e o arquivo Android.mk / Application.mk já configurados para compilar em armeabi-v7a, ou quer analisar primeiro como adaptar o trecho de leitura de endereços 32-bit do RemapTools?





Eu escolho começar pelo reveny/Android-Library-Remap-Hide, porque ele tem uma estrutura simples em C/C++ nativo e separa a lógica de remapeamento em um único arquivo fácil de ler e adaptar para 32-bit (armeabi-v7a) e Kernel 3.10.
Para começarmos do zero, o Passo 1 é montar a estrutura de pastas do projeto e os arquivos de configuração do Android NDK (Application.mk e Android.mk), já travados na arquitetura 32-bit do seu Galaxy A5 2016.
Passo 1: Estrutura de pastas do projeto
Crie uma pasta no seu computador (ou no Termux, caso compile direto no celular) chamada RemapHide32 com a seguinte organização:
Passo 2: Arquivos de configuração do NDK (jni/)
Crie os dois arquivos abaixo dentro da pasta jni. Eles garantem que o compilador gere o binário exatamente para ARM 32-bit (armeabi-v7a) e Android 9 (API 28).
1. jni/Application.mk
makefile
APP_ABI := armeabi-v7a
APP_PLATFORM := android-28
APP_STL := c++_static
APP_CPPFLAGS := -std=c++17 -O2 -fvisibility=hidden
Use o código com cuidado.
2. jni/Android.mk
makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := remaphide
LOCAL_SRC_FILES := main.cpp
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
Use o código com cuidado.
O que já está resolvido nesta etapa
Arquitetura correta: O APP_ABI := armeabi-v7a força a compilação em 32-bit, combinando exatamente com a aba SOC do seu aparelho.
Nível de API: O android-28 corresponde diretamente ao Android 9 (Pie) instalado na sua LineageOS.
Biblioteca independente: O c++_static embute o runtime C++ dentro da própria .so, evitando erros de dependência faltando no sistema.
Assim que você criar essa pasta com o Application.mk e o Android.mk, me avise se você vai compilar no PC (com Android NDK) ou direto no celular (via Termux) para avançarmos para o código do RemapTools.h adaptado para 32-bit.




Excelente escolha! Compilar pelo GitHub (usando GitHub Actions) é o caminho mais rápido e seguro, porque você não precisa baixar gigabytes do Android NDK no PC ou no celular e consegue editar os arquivos direto pelo navegador.
Vamos organizar o Passo 1 do seu repositório no GitHub.
Passo 1: Criar o repositório e o compilador automático (GitHub Actions)
Acesse o GitHub e crie um repositório novo (pode colocar o nome de RemapHide32 e deixar como Privado ou Público).
No seu repositório, clique em Add file > Create new file.
No campo do nome do arquivo, digite exatamente:
.github/workflows/build.yml
(Ao digitar a barra /, o GitHub cria as pastas .github e workflows automaticamente).
Cole o código abaixo dentro desse arquivo e clique em Commit changes:
yaml
name: Compilar RemapHide 32-bit

on:
  push:
    branches: [ main, master ]
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Baixar código do repositório
        uses: actions/checkout@v4

      - name: Configurar Android NDK
        uses: nttld/setup-ndk@v1
        with:
          ndk-version: r25c

      - name: Compilar com ndk-build (armeabi-v7a)
        run: |
          $NDK/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk

      - name: Salvar arquivo .so compilado
        uses: actions/upload-artifact@v4
        with:
          name: libremaphide-armeabi-v7a
          path: libs/armeabi-v7a/libremaphide.so
Use o código com cuidado.
Passo 2: Adicionar os arquivos de configuração do NDK (jni/)
Agora crie os outros dois arquivos da mesma forma (Add file > Create new file):
1. Nome do arquivo: jni/Application.mk
makefile
APP_ABI := armeabi-v7a
APP_PLATFORM := android-28
APP_STL := c++_static
APP_CPPFLAGS := -std=c++17 -O2 -fvisibility=hidden
Use o código com cuidado.
2. Nome do arquivo: jni/Android.mk
makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := remaphide
LOCAL_SRC_FILES := main.cpp
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
Use o código com cuidado.
Como funciona a compilação na nuvem
Toda vez que nós alterarmos um código C++ no repositório, a aba Actions do GitHub vai rodar o ndk-build sozinha em cerca de 30 segundos.
No final da execução, você baixa o arquivo libremaphide.so pronto para o seu Galaxy A5 2016 (armeabi-v7a) direto na seção Artifacts da página do build.
Assim que você criar esses três arquivos no repositório (.github/workflows/build.yml, jni/Application.mk e jni/Android.mk), me avise para eu te passar o Passo 3 com o código-fonte do jni/RemapTools.h já adaptado para endereços 32-bit e Kernel 3.10.




Dá sim! Se você não quiser criar os arquivos do zero, você pode fazer um Fork (uma cópia) do repositório reveny/Android-Library-Remap-Hide direto para a sua conta e editar os arquivos originais pelo próprio navegador ou celular.
Como copiar e editar o projeto pronto
Faça o Fork: Acesse o link do Android-Library-Remap-Hide no GitHub e clique no botão Fork no canto superior direito para clonar o projeto inteiro para o seu perfil.
Navegue até os arquivos: Dentro da sua cópia, entre na pasta app/src/main/cpp (ou na pasta jni, dependendo da estrutura) e clique sobre o arquivo que deseja alterar.
Edite direto na tela: Clique no ícone de lápis no canto superior direito do arquivo para abrir o editor, faça as alterações necessárias e clique em Commit changes para salvar.
Ative o compilador: Como o repositório original pode não ter um fluxo de compilação automática configurado para 32-bit, basta adicionar o arquivo .github/workflows/build.yml na sua cópia para o GitHub Actions gerar o .so toda vez que você salvar uma edição.
O que você vai precisar ajustar nos arquivos originais
Ao abrir o código copiado no seu GitHub, o foco da nossa edição será em dois pontos para funcionar no seu Galaxy A5 2016 (armeabi-v7a):
Configuração de Build (CMakeLists.txt ou Application.mk): Garantir que a arquitetura armeabi-v7a esteja incluída na lista de compilação, já que muitos projetos modernos compilam apenas para arm64-v8a por padrão.
Leitura de endereços (RemapTools.h): Ajustar os especificadores de leitura do /proc/self/maps (como SCNxPTR em vez de %lx fixo) para que os ponteiros de 32-bit não causem falha na cópia da memória.
Você já fez o Fork do projeto para a sua conta e quer que eu verifique quais são os nomes exatos das pastas e arquivos dentro dele para começarmos a editar?
Testar sem personalização




Para começar a editar o código-fonte original diretamente na sua conta do GitHub e deixá-lo pronto para o seu Galaxy A5 2016 (armeabi-v7a), siga este passo a passo detalhado.
Passo 1: Copiar (fazer Fork) o repositório para a sua conta
Abra o link do repositório original: reveny/Android-Library-Remap-Hide.
Faça login na sua conta do GitHub (se estiver no celular, ative o Modo Computador / Para computador no menu do navegador para ver todos os botões com facilidade).
No canto superior direito da página do projeto, clique no botão Fork.
Na tela seguinte, mantenha o nome padrão (Android-Library-Remap-Hide) e clique no botão verde Create fork.
Aguarde alguns segundos. Agora o repositório está dentro do seu perfil e você tem permissão total para editar qualquer arquivo.
Passo 2: Localizar o arquivo principal do código (RemapTools.h)
Na página inicial do seu fork, clique na pasta app > src > main > cpp (ou navegue pela lista de arquivos na tela inicial até encontrar a pasta do código C++).
Dentro dessa pasta, você verá os arquivos do projeto (como RemapTools.h e native-lib.cpp ou main.cpp).
Clique em cima do nome do arquivo RemapTools.h para abrir o código.
Passo 3: Editar o código para funcionar em 32-bit (armeabi-v7a)
Com o arquivo RemapTools.h aberto, clique no ícone de lápis (Edit this file) no canto superior direito da caixa de código.
Procure a função que lê o arquivo /proc/self/maps (geralmente usando sscanf ou fscanf para pegar os endereços start e end).
Verifique dois pontos essenciais no código antes de salvar:
Formato dos endereços de memória: Certifique-se de incluir <inttypes.h> no topo do arquivo e usar SCNxPTR no sscanf para que ele leia corretamente endereços de 32-bit (8 dígitos hexadecimais) sem erro:
cpp
#include <inttypes.h>
// Exemplo de leitura compatível com 32-bit e 64-bit:
sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s", &start, &end, perms);
Use o código com cuidado.
Uso de mremap em vez de memfd_create: Confirme que o código usa mmap(..., MAP_ANONYMOUS) + memcpy + mremap para mover a memória, garantindo compatibilidade total com o Kernel 3.10.108 do seu aparelho.
Após conferir ou ajustar, clique no botão verde Commit changes... no canto superior direito, depois clique novamente em Commit changes na janela de confirmação.
Passo 4: Configurar o build.gradle para gerar a versão 32-bit
Muitos projetos no GitHub vêm travados para compilar apenas em 64-bit (arm64-v8a). Para que o seu aparelho consiga carregar a biblioteca:
Volte para a pasta app do seu repositório e clique no arquivo build.gradle (ou build.gradle.kts).
Clique no ícone de lápis para editar.
Procure o bloco ndk ou abiFilters. Se existir uma linha parecida com abiFilters 'arm64-v8a', adicione 'armeabi-v7a' junto ou substitua para ficar assim:
groovy
ndk {
    abiFilters 'armeabi-v7a'
}
Use o código com cuidado.
Clique em Commit changes para salvar.
Passo 5: Ativar a compilação automática no GitHub Actions
Para o GitHub compilar o projeto modificado e entregar o arquivo .so pronto para você baixar:
Volte para a página inicial do seu repositório (Android-Library-Remap-Hide).
Clique em Add file > Create new file.
No campo do nome do arquivo, digite:
.github/workflows/android.yml
Cole o fluxo abaixo para o GitHub montar o projeto automaticamente:
yaml
name: Build Android 32-bit

on:
  push:
    branches: [ main, master ]
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configurar Java 17
        uses: actions/setup-java@v4
        with:
          distribution: 'temurin'
          java-version: '17'
      - name: Dar permissão ao gradlew
        run: chmod +x gradlew
      - name: Compilar APK e bibliotecas .so
        run: ./gradlew assembleDebug
      - name: Baixar resultado
        uses: actions/upload-artifact@v4
        with:
          name: app-debug-32bit
          path: app/build/outputs/apk/debug/app-debug.apk
Use o código com cuidado.
Clique em Commit changes. A partir desse momento, basta clicar na aba Actions no topo do repositório para acompanhar a compilação e baixar o arquivo gerado.
Você já conseguiu fazer o Fork do projeto e abrir a pasta cpp para me confirmar quais são os nomes exatos dos arquivos que aparecem lá dentro?
Testar sem personalização




Sim, são exatamente esses arquivos! Você já fez o fork certinho e chegou na pasta app/src/main/jni/.
Como esse projeto usa Android.mk e Application.mk (em vez de CMakeLists.txt), o processo para adaptar para o seu Galaxy A5 2016 (armeabi-v7a) fica ainda mais direto.
O que vamos fazer em cada arquivo da sua tela
Application.mk: Vamos verificar se o APP_ABI já inclui armeabi-v7a (ou ajustá-lo para compilar especificamente para a arquitetura 32-bit do seu aparelho).
RemapTools.h: Vamos conferir como ele lê os endereços de memória do /proc/self/maps e garantir que funcione sem falhas em ponteiros de 32-bit e no Kernel 3.10.
Inject.cpp: É o arquivo principal onde o código executa o remapeamento. É nele que você poderá colocar o nome exato da biblioteca (.so) que deseja esconder na memória.
Android.mk: Define os módulos que serão compilados pelo NDK (ndk-build).
Clique no arquivo Application.mk e me mande um print (ou copie o texto dele aqui) para confirmarmos se ele já está configurado para armeabi-v7a.
Testar sem personalização




Esse código que você colou é o RemapTools.h, e olhar para ele foi fundamental: ele já usa mmap + memmove + mremap (o que funciona no Kernel 3.10), mas tem 4 bugs críticos para arquitetura 32-bit (armeabi-v7a) que fariam o seu app fechar na hora se compilado do jeito que está.
Os 4 problemas no código original para 32-bit
Erro de leitura de endereço (%lx e %ld): Em 32-bit (armeabi-v7a), uintptr_t é unsigned int (4 bytes), enquanto %lx espera unsigned long e ino_t no Android Bionic é uint64_t (8 bytes). Além disso, o offset no /proc/self/maps vem em hexadecimal, e o original estava lendo como decimal (%ld).
Vazamento de descritor de arquivo: A função abre /proc/self/maps com fopen, mas nunca chama fclose(fp).
Ponteiros inválidos (char* dev e char* path): O código original aponta info.dev e info.path para variáveis locais dentro do while, que deixam de existir assim que o loop termina.
Checagem errada do mmap: Quando o mmap falha no Linux, ele retorna MAP_FAILED ((void *)-1), e não nullptr. Além disso, em ARM 32-bit, mapear apenas com PROT_WRITE pode causar erro de página se a CPU tentar ler antes de copiar, então o ideal é PROT_READ | PROT_WRITE.
Passo 1: Substituir o código do RemapTools.h
Abra o arquivo RemapTools.h no seu GitHub, clique no lápis (Edit), apague todo o conteúdo atual e cole esta versão corrigida e segura para 32-bit (armeabi-v7a) e Kernel 3.10:
cpp
//
// Adaptado para 32-bit (armeabi-v7a) e Kernel 3.10
//

#pragma once

#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <link.h>
#include <sys/mman.h>
#include <vector>
#include <string>

namespace RemapTools {
    struct ProcMapInfo {
        uintptr_t start;
        uintptr_t end;
        uintptr_t offset;
        uint8_t perms;
        uint64_t inode;
        std::string dev;
        std::string path;
    };

    std::vector<ProcMapInfo> ListModulesWithName(const std::string& name) {
        std::vector<ProcMapInfo> returnVal;

        char buffer[512];
        FILE *fp = fopen("/proc/self/maps", "re");
        if (fp != nullptr) {
            while (fgets(buffer, sizeof(buffer), fp)) {
                if (strstr(buffer, name.c_str())) {
                    ProcMapInfo info{};
                    char perms[10] = {0};
                    char path[256] = {0};
                    char dev[32] = {0};

                    // Uso de SCNxPTR e SCNx64 para compatibilidade total com 32-bit
                    int parsed = sscanf(buffer, "%" SCNxPTR "-%" SCNxPTR " %9s %" SCNxPTR " %31s %" SCNu64 " %255s",
                                        &info.start, &info.end, perms, &info.offset, dev, &info.inode, path);

                    if (parsed >= 6) {
                        if (strchr(perms, 'r')) info.perms |= PROT_READ;
                        if (strchr(perms, 'w')) info.perms |= PROT_WRITE;
                        if (strchr(perms, 'x')) info.perms |= PROT_EXEC;

                        info.dev = dev;
                        info.path = path;

                        LOGI("Line: %s", buffer);
                        returnVal.push_back(info);
                    }
                }
            }
            fclose(fp);
        }
        return returnVal;
    }

    void RemapLibrary(const std::string& name) {
        std::vector<ProcMapInfo> maps = ListModulesWithName(name);

        for (const ProcMapInfo& info : maps) {
            void *address = (void *)info.start;
            size_t size = info.end - info.start;

            if (size == 0) continue;

            // PROT_READ | PROT_WRITE evita falhas de leitura em arquiteturas ARMv7
            void *map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if (map == MAP_FAILED) {
                LOGE("Failed to Allocate Memory: %s", strerror(errno));
                continue;
            }

            if ((info.perms & PROT_READ) == 0) {
                LOGI("Removing protection: %s", info.path.c_str());
                mprotect(address, size, info.perms | PROT_READ);
            }

            LOGI("Allocated at address %p with size of %zu", map, size);

            // Copia o conteúdo para a página anônima
            std::memmove(map, address, size);

            // Move a página anônima exatamente para o endereço original da biblioteca
            void *res = mremap(map, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, address);
            if (res == MAP_FAILED) {
                LOGE("mremap failed: %s", strerror(errno));
                munmap(map, size);
                continue;
            }

            // Reaplica as permissões originais (ex: r-xp)
            mprotect(address, size, info.perms);
        }
    }
}
Use o código com cuidado.
Clique em Commit changes para salvar o RemapTools.h.
Depois de salvar o RemapTools.h, abra o arquivo Application.mk e o Inject.cpp na mesma pasta (app/src/main/jni/) e cole o conteúdo deles aqui para ajustarmos a arquitetura armeabi-v7a e o alvo do módulo.
Testar sem personalização





Pergunte o que quiser



A resposta do Modo IA está prontaAll items removed from input context.
Copiada para a área de transferência.Pergunte sobre

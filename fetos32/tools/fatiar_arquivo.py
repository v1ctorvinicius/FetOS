import os

def fatiar_arquivo(nome_arquivo, num_partes):
    if not os.path.exists(nome_arquivo):
        print(f"Erro: O arquivo '{nome_arquivo}' não foi encontrado.")
        return

    with open(nome_arquivo, 'r', encoding='utf-8') as f:
        linhas = f.readlines()

    total_linhas = len(linhas)
    tamanho_fatia = total_linhas // num_partes
    linhas_extras = total_linhas % num_partes

    inicio = 0
    for i in range(num_partes):
        fim = inicio + tamanho_fatia + (1 if i < linhas_extras else 0)
        
        nome_saida = f"parte_{i+1}_{nome_arquivo}"
        
        with open(nome_saida, 'w', encoding='utf-8') as f_out:
            f_out.writelines(linhas[inicio:fim])
        
        print(f"Criado: {nome_saida} com {fim - inicio} linhas.")
        inicio = fim

arquivo_alvo = 'FetOS32_SourceCode.txt'
quantidade_partes = 5

fatiar_arquivo(arquivo_alvo, quantidade_partes)

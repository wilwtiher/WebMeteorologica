# Aluno: Wilton Lacerda Silva Júnior
## Matrícula: TIC370100193
# Video explicativo: https://youtu.be/yzyqFzzOnpQ
# Projeto Monitoramento de Sensores
O objetivo do projeto é utilizar de sensores com barramento I2C para verificar valores e informar para o usuário de diversas maneiras, além de informar quando esses valores passar do limite definido.
## Funcionalidades

- **LED RGB**
  - O LED central da placa RGB serve como um aviso, variando sua cor entre verde e vermelho de acordo com as variáveis e o limite.
- **BUZZER**
	- O buzzer funciona da mesma forma, ligando caso os valores passem dos informados nos limites.
- **Display OLED**
	- O display OLED mostrará informações na própria placa em tempo real de qual o valor de cada variável do sensor.
- **Sensores**
	- O sensor aht20 e bmp280 servirá para extrair informações do meio, como os valores ditos anteriormente.
- **INTEGRAÇÃO WEB**
   - A integração com a WEB via html permitirá verificar os valores das variáveis em diversos dispositivos, além de alterar os limites.
- **Display OLED**
   - O display OLED mostrará na própria placa os valores captados e tratados em tempo real.

# Requisitos
## Hardware:

- Raspberry Pi Pico W.
- 1 buzzer no pino 10.
- 1 LED vermelho no pino 13.
- 1 LED verde no pino 11.
- 1 display OLED no I2C dos pinos 14 e 15.
- 1 sensor aht20 e bmp280 no I2C da placa, no endereço 0x3C

## Software:

- Ambiente de desenvolvimento VS Code com extensão Pico SDK.
- Configuração do código para conexão de rede.

# Instruções de uso
## Configure o ambiente:
- Certifique-se de que o Pico SDK está instalado e configurado no VS Code.
- Compile o código utilizando a extensão do Pico SDK.
- Altere a senha e a rede no programa para a escolhida a ser usada.
## Teste:
- Utilize a placa BitDogLab para o teste. Caso não tenha, conecte os hardwares informados acima nos pinos correspondentes.

# Explicação do projeto:
## Contém:
- O projeto terá uma forma de comunicação direta com o usuário: o meio WEB.
- LED RGB e Buzzer para avisar de maneira visual e sonora quando os valores do sensor passar do limite definido.
- Também contará com uma saída visual na placa, o display OLED.

## Funcionalidades:
- O programa captara os valores da temperatura, umidade e pressão pelo sensor.
- O programa emitira aviso sonoro e visual caso esses valores passem do limite ou fiquem abaixo do limite mínimo.
- O programa acessara a internet via rede local.
- O programa criará um servidor WEB na rede local.
- O servidor WEB mostrará os valores captados pelo sensor e permitirá alterar os valores de limite.
- O usuário poderá visualizar esses valores pelo display OLED.

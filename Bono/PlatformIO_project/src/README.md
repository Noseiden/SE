# Agitador de Muestras con Control Embebido

## Descripción

Este proyecto desarrolla un prototipo funcional de un sistema de agitación controlada para homogeneización de muestras de sangre, basado en un microcontrolador ESP32 y una interfaz de potencia para accionamiento de un motor DC. La solución permite regular la intensidad de agitación, seleccionar el sentido de giro y visualizar en tiempo real el porcentaje de potencia aplicado al motor.

## Hardware Implementado

El sistema integra:

- ESP32 como unidad de control.
- Motor DC de 12V para el mecanismo de agitación.
- Etapa de potencia con MOSFETs, relés y optoacopladores para inversión de giro y protección.
- Potenciómetro para ajuste de velocidad.
- Pulsadores para selección de dirección.
- Indicadores LED para señalización del sentido de giro.
- Displays de 7 segmentos para visualización del porcentaje de potencia.

## Funcionalidades

- Control de velocidad mediante PWM.
- Cambio de giro del motor.
- Protección ante inversión abrupta de dirección.
- Visualización de potencia en porcentaje.
- Aislamiento entre etapa de control y etapa de potencia.
# IIC2333 - Tarea 2

**Cristóbal Eyzaguirre  15636976**
**Franco Mendez         14636077**

## Diseño de myShell

myShell cuenta con un solo módulo con varias funciones.
Realiza un loop en `main`, desde donde se llama a las funciones principales
que leen los comandos ingresados por el usuario, los parsean en argumentos que
se guardan en punteros con un puntero hacia cada uno de ellos, y los entregan
de cierta manera (según estos mismos) para que se ejecuten.

Para ejecutar un comando se llama a la función `execArgs`, que verifica que
no hayan palabras especiales. Si no hay, se ejecuta un simple `execv`,
cersiorándose de que si hay un `customPath` y exista un archivo en la ruta
`customPath + arg[0]` sea ejecutado, de no ser así, se corre de manera absoluta.

En el caso en que sí hayan palabras especiales se llaman funciones que modelan
el comportamiento de cada una. Dentro de las no triviales se encuentra la que
se activa al haber un `&` como último argumento, que se encarga de crear tantos
procesos como indique el número que le sucede (o 1 sí no hay) para que ejecuten
los argumentos con `execv` (verificando el path personalizado) y el programa
principal pueda seguir corriendo.

Para el manejo de los estados anteriores, prompt y path personalizados, se usan
variables globales.


## Diseño de Life

El diseño de life es bastante simple. Se crean tantos procesos como cores tenga el computador y se usa la variable `core` en la iteracion sobre `cores` guardar el "indice" de cada proceso de 0 a N-1 donde N es el numero de cores.

Para cada proceso se guarda en `status` su pid en la posicion que corresponde a su indice. Para que `master` tenga los estados y numeros de iteraciones de cada proceso `slave` existen en memoria compartida `iterations` que guarda en el indice de cada proceso esclavo el numero de iteraciones que ha corrido, y `states` que almacena las matrices que representan el ultimo estado computado por completo en el pointer con desfase `indice * (filas * columnas * sizeof(int))`.

Para coordinar los procesos existe una ultima variable en memoria compartida `shared`. Esta hace que solo un proceso (el primero en terminar) imprima su resultado en *CONSOLA*, y elimina al resto.

Los threads no requieren coordinacion porque cada uno edita sus propias filas (que son distribuidas equitativamente al principio) y por lo mismo nunca intentan cambiar valores de la misma variable al mismo tiempo. Se usa un join para sincronizar los threads y esperar a que terminen todos antes de pasar a la siguiente iteracion de el juego de la vida.

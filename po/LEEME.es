[ This file explains why there are various incomplete es_??.po in adition
  to the es.po; that is on purpose ]

Los diferentes archivos para las locales es_DO, es_GT, es_HN, es_MX, es_PA,
es_PE y es_SV; solo existen porque en esos paises se usa una representación
de los números diferente de la que se usa en los demás paises de habla
castellan (usan el sistema anglosajón, esdecir 1,000 en vez de 1.000 para 
'mil').

Por ello solo es necesario traducir aquellas ocurrencias donde aparezcan
números; no hace falta perder el tiempo traduciendo en doble lo demás;
el sistema de internacionalización de GNU gettext es lo suficientemente
inteligente como para buscar las traducciones en la locale 'es' cuando no
las encuentra en una 'es_XX' más específica.

También podriase crear un es_ES para traducir aquellos terminos que difieren
en España y América (ej: computadora/ordenador, archivo/fichero, ícono/icono),
si algún español tiene ganas de encargarse de ello, bienvenido.
Notese que solo es necesario traducir aquellas cadenas de texto para las
cuales se usan terminos diferentes en España; no es necesario traducir las
demás, con que estén en la traducción general 'es' ya basta. 
 
Pablo Saratxaga
<srtxg@chanae.alphanet.ch>



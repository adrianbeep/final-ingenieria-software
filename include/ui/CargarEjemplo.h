#pragma once

#include <string>

// ---------------------------------------------------------------------------
// La carga de los DATOS DE EJEMPLO.
//
// QUE ES
//
// Una operacion que copia la SEMILLA (`data/ejemplo/*.dat`) sobre los
// archivos de trabajo (`data/*.dat`) y recarga los repositorios.
//
// POR QUE ES UN HELPER Y NO UNA FUNCION DE CADA PANTALLA
//
// La copia y la recarga las necesitan TRES pantallas (la de configuracion y
// los dos listados vacios: el de pacientes y el de turnos). Si cada una
// copiara por su cuenta, habria TRES reglas que se pueden desincronizar
// (por ejemplo, una que copiara antes de recargar y otra al reves, que es
// justamente el error que destroys los datos: `cargar()` sobre un archivo
// inexistente lo crea VACIO).
//
// LA LOGICA COMPARTIDA VIVE ACA
//
// La pantalla que la usa decide CUANDO se carga (y pide la confirmacion de
// dos pasos); el helper decide COMO se copia y COMO se recarga.
//
// NUNCA LANZA
//
// El `catch` global de `main` solo catchea `ExcepcionSistema`: una
// `filesystem_error` o un `bad_alloc` que se escaparan matarían el proceso
// con SIGABRT en vez de con codigo 0. Por eso TODO el cuerpo del helper esta
// dentro de un `try`, y el helper devuelve `false` y el motivo en `error`.
//
// LA COPIA ES ATOMICA, POR ARCHIVO
//
// La copia va a un `.tmp` en el MISMO directorio y despues se hace `rename`
// (el mismo patron que usa `ArchivoTexto::guardar()`): el archivo vivo
// esta viejo o nuevo, nunca truncado a mitad de copia.
//
// La semilla NO se toca: la copia va de la semilla al archivo vivo, nunca al
// reves.
// ---------------------------------------------------------------------------
namespace ui {

class Contexto;

// Copia la semilla sobre los archivos vivos y RECARGA los repositorios del
// contexto.
//
// Devuelve true si salio bien. Si no, `error` dice por que (y queda vacio
// si salio bien).
//
// EL ORDEN ES OBLIGATORIO: COPIAR PRIMERO, RECARGAR DESPUES.
//
// `cargar()` sobre un archivo que no existe lo CREA VACIO y borra el arbol
// en memoria: recargar antes de copiar perderia los datos.
//
// LA RECARGA ES EN EL LUGAR
//
// Se recarga el MISMO objeto que las pantallas tienen por referencia: no se
// recrea el contexto ni el servicio de turnos, asi que todas las pantallas
// ven los datos nuevos sin enterarse.
//
// CADA REPOSITORIO SE RECARGA POR SEPARADO
//
// Si uno falla, los otros dos recargan igual: si no, el que falla deja a los
// otros dos con los datos viejos en memoria, y la proxima `persistir()` de un
// alta pisaria el archivo nuevo con la memoria vieja (la carga se revertiria
// en silencio). Si alguno falla, la carga NO se da por buena.
bool cargarDatosDeEjemplo(Contexto& contexto, std::string& error);

} // namespace ui

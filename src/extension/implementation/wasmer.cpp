
#include "wasmer.h"

namespace Inkscape {
namespace Extension {
namespace Implementation {

    bool Wasmer::load(Inkscape::Extension::Extension * /*module*/){return false;}
    void Wasmer::unload(Inkscape::Extension::Extension * /*module*/){}

    bool Wasmer::check(Inkscape::Extension::Extension * /*module*/){return false;}

    bool Wasmer::cancelProcessing () {return false;}
    void Wasmer::effect(Inkscape::Extension::Effect * /*module*/,
                        Inkscape::UI::View::View * /*document*/,
                        ImplementationDocumentCache * /*docCache*/){}

}  // namespace Implementation
}  // namespace Extension
}  // namespace Inkscape


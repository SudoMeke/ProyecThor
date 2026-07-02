#pragma once
#include "LibraryContext.h"

// Necesitamos ItemType para RenderDefaultStyleCombo
#include "backend/core/PresentationCore.h"

namespace ProyecThor::Library {

// Modal de renombrar archivo o URL.
// Muta ctx.showRenameModal, ctx.streamURLs, ctx.selectedURLIndex,
// ctx.selectedIndex, y llama ctx.refreshList() / ctx.saveStreamURLs().
void RenderRenameModal(LibraryContext& ctx);

// Combo de estilo por defecto para Songs y Bibles.
// Solo se renderiza si currentCategoryInt == Songs o Bibles.
// currentCategoryInt se interpreta como LibraryCategory.
void RenderDefaultStyleCombo(LibraryContext& ctx);

} // namespace ProyecThor::Library
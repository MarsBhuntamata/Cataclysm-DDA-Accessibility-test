#pragma once
#ifndef CATA_SRC_STATUS_BROWSER_H
#define CATA_SRC_STATUS_BROWSER_H

class avatar;

/**
 * Modal, screen-reader friendly view of the sidebar.
 *
 * One tab per enabled sidebar panel in sidebar order; one row per widget.
 * Rows are spoken as the focus moves, Left/Right or Tab switch sections,
 * READ_SECTION reads the whole section, READ_ALL_SECTIONS reads everything,
 * and the last visited section is restored the next time it opens.
 */
void show_status_browser( const avatar &ava );

#endif // CATA_SRC_STATUS_BROWSER_H

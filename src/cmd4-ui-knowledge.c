/*
 * Additional knowledge menu helpers kept separate from the Angband-derived
 * command flow in cmd4.c.
 */

static cptr knowledge_menu_labels[] = { "(1) Display known artefacts",
    "(2) Display known monsters", "(3) Display known objects",
    "(4) Display names of the fallen", "(5) Display kill counts",
    "(6) Display character notes file" };

static cptr knowledge_menu_details[] = {
    "Browse artefacts you have discovered and recall their properties.",
    "Browse known monsters and review their recall information.",
    "Browse known object groups and inspect individual items.",
    "Show the recorded names of the fallen.",
    "Display a sorted record of slain creatures.",
    "Open the character notes file if one exists." };

static int knowledge_menu_aux(int* highlight)
{
    char ch;
    int i;
    char menu_text[512];
    byte menu_attrs[512];
    char details_text[512];
    byte details_attrs[512];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;

    ui_text_builder_init(&menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, details_text, details_attrs, sizeof(details_text));

    ui_text_builder_append_line(
        &menu_builder, "Display current knowledge", TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Use the keyboard or mouse to choose a knowledge screen.", TERM_SLATE);

    if ((*highlight >= 1) && (*highlight <= 6))
    {
        ui_text_builder_append_line(
            &details_builder, knowledge_menu_labels[*highlight - 1], TERM_L_WHITE);
        ui_text_builder_newline(&details_builder, TERM_WHITE);
        ui_text_builder_append_line(&details_builder,
            knowledge_menu_details[*highlight - 1], TERM_SLATE);
    }

    ui_menu_begin();
    for (i = 0; i < 6; i++)
    {
        bool selected = (*highlight == i + 1);

        Term_putstr(5, 4 + i, -1, selected ? TERM_L_BLUE : TERM_WHITE,
            knowledge_menu_labels[i]);
        ui_menu_add(5, 4 + i, (int)strlen(knowledge_menu_labels[i]), 1, '\r',
            selected, TERM_WHITE, knowledge_menu_labels[i]);
    }
    ui_menu_set_text(menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details(
        details_text, details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_end();

    Term_fresh();
    Term_gotoxy(5, 3 + *highlight);

    hide_cursor = TRUE;
    ch = inkey();
    hide_cursor = FALSE;

    if ((ch >= '1') && (ch <= '6'))
    {
        *highlight = (int)ch - '0';
        ui_menu_clear();
        return *highlight;
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' '))
    {
        ui_menu_clear();
        return *highlight;
    }

    if (ch == '8')
    {
        (*highlight)--;
        if (*highlight < 1)
            *highlight = 6;
        return -1;
    }

    if (ch == '2')
    {
        (*highlight)++;
        if (*highlight > 6)
            *highlight = 1;
        return -1;
    }

    if (ch == ESCAPE)
    {
        ui_menu_clear();
        return 0;
    }

    return -1;
}

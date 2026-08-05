# SPDX-License-Identifier: EUPL-1.2
#
# Copyright (c) 2026 Fernando Carmona Varo
#
# Asciidoctor PDF extension for the Sil-Quest manual.
#
# The stock converter treats a managed TOC as a reserved front-matter extent,
# especially when the document also has a title page. This tiny extension keeps
# the real generated TOC, including page numbers, then adds the welcome/licence
# note as a footer-like block on the final TOC page.

require 'asciidoctor/pdf'

module SilQuestPdfTocNote
  def ink_toc doc, num_levels, toc_page_number, start_cursor, num_front_matter_pages = 0
    toc_page_nums = super
    add_sil_quest_toc_note doc, toc_page_nums unless scratch?
    toc_page_nums
  end

  private

  def add_sil_quest_toc_note doc, toc_page_nums
    note_path = doc.attr 'silquest-toc-note-file'
    return if note_path.nil_or_empty?

    note_path = ::File.expand_path note_path, (doc.attr 'docdir')
    return unless ::File.readable? note_path

    note_text = sil_quest_plain_note ::File.read note_path, mode: 'r:utf-8'
    return if note_text.empty?

    original_page = page_number
    go_to_page toc_page_nums.end

    note_height = 74
    note_top = note_height + 6
    stroke_color 'A78B62'
    line_width 0.45
    stroke_horizontal_line 0, bounds.width, at: note_top

    fill_color '6C6257'
    font @theme.base_font_family, size: 7.1, style: :italic do
      text_box note_text,
        at: [0, note_height],
        width: bounds.width,
        height: note_height - 8,
        leading: 1.1,
        overflow: :shrink_to_fit
    end

    fill_color @font_color
    stroke_color @theme.base_border_color || '000000'
    go_to_page original_page
  end

  def sil_quest_plain_note asciidoc
    paragraphs = []
    current = []

    asciidoc.each_line do |line|
      stripped = line.strip
      next if stripped.start_with? '//'
      next if stripped.start_with? '[.'

      if stripped.empty?
        paragraphs << current.join(' ') unless current.empty?
        current = []
      else
        current << stripped
      end
    end

    paragraphs << current.join(' ') unless current.empty?
    paragraphs
      .map {|paragraph| paragraph.gsub(/`([^`]+)`/, '\1') }
      .join "\n\n"
  end
end

Asciidoctor::PDF::Converter.prepend SilQuestPdfTocNote

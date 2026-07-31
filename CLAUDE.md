# Project

ESPHome external component for the Texas Instruments LDC1314.

# Goal

Create a generic ESPHome driver.

Do NOT implement watermeter logic.

# Architecture

The driver only exposes raw measurements.

Application-specific algorithms remain outside the component.

# References

docs/LDC1314_datasheet.pdf
docs/snaa221b.pdf
docs/snoa950.pdf
docs/snoa959.pdf
docs/snoa945.pdf
docs/SNOA930_Application_note_TI.com.html
docs/SNOSCZ0_Data_sheet_TI.com.html

.plan

Always follow current ESPHome architecture.

Never invent custom frameworks.

Reuse existing ESPHome component patterns whenever possible.

## Documentation policy

Never use the original PDF documents unless information is missing from the generated Markdown version.

Prefer:
1. docs/knowledge_base.md
2. docs/summaries/
3. *.md documents
4. Original PDFs (only as a last resort)
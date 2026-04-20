# Enterprise Deployment Guide

## Doel

Standaard deploymentbeleid voor enterprise-omgevingen op Windows/macOS.

## Ondersteunde deploymentpatronen

- Windows rollout via MSI/NSIS distributiekanalen
- macOS rollout via PKG/DMG met beheerplatform
- Silent install/uninstall voor beheerde endpoints
- Configuratiebeheer integratie (Intune/Jamf/SCCM)

## Uitrolvereisten

- Versiepinnen + rollout rings (pilot -> broad)
- Rollback pad per platform gedefinieerd
- Installatie- en activatielogs centraal beschikbaar voor support

## Governance status

**Status (2026-04-20):** enterprise deployment policy vastgesteld; per klantomgeving wordt een implementatieprofiel vastgelegd.

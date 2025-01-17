// Copyright © SixtyFPS GmbH <info@slint-ui.com>
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Slint-commercial

use i_slint_compiler::diagnostics::Spanned;
use i_slint_compiler::langtype::{ElementType, Type};
use i_slint_compiler::object_tree::{Element, ElementRc};
use i_slint_compiler::parser::{syntax_nodes, SyntaxKind};
use std::collections::HashSet;

#[cfg(target_arch = "wasm32")]
use crate::wasm_prelude::*;

#[derive(serde::Deserialize, serde::Serialize, Debug, PartialEq)]
pub(crate) struct DefinitionInformation {
    property_definition_range: lsp_types::Range,
    expression_range: lsp_types::Range,
}

#[derive(serde::Deserialize, serde::Serialize, Debug, PartialEq)]
pub(crate) struct DeclarationInformation {
    uri: lsp_types::Url,
    start_position: lsp_types::Position,
}

#[derive(serde::Deserialize, serde::Serialize, Debug, PartialEq)]
pub(crate) struct PropertyInformation {
    name: String,
    type_name: String,
    declared_at: Option<DeclarationInformation>,
    defined_at: Option<DefinitionInformation>, // Range in the elements source file!
    group: String,
}

#[derive(serde::Deserialize, serde::Serialize, Debug)]
pub(crate) struct ElementInformation {
    id: String,
    type_name: String,
}

#[derive(serde::Deserialize, serde::Serialize)]
pub(crate) struct QueryPropertyResponse {
    properties: Vec<PropertyInformation>,
    element: Option<ElementInformation>,
    source_uri: Option<String>,
}

impl QueryPropertyResponse {
    pub fn no_element_response(uri: String) -> Self {
        QueryPropertyResponse { properties: vec![], element: None, source_uri: Some(uri) }
    }
}

// This gets defined accessibility properties...
fn get_reserved_properties<'a>(
    group: &'a str,
    properties: &'a [(&'a str, Type)],
) -> impl Iterator<Item = PropertyInformation> + 'a {
    properties.iter().map(|p| PropertyInformation {
        name: p.0.to_string(),
        type_name: format!("{}", p.1),
        declared_at: None,
        defined_at: None,
        group: group.to_string(),
    })
}

fn source_file(element: &Element) -> Option<String> {
    element.source_file().map(|sf| sf.path().to_string_lossy().to_string())
}

fn get_element_properties<'a>(
    element: &'a Element,
    offset_to_position: &'a mut dyn FnMut(u32) -> lsp_types::Position,
    group: &'a str,
) -> impl Iterator<Item = PropertyInformation> + 'a {
    let file = source_file(element);

    element.property_declarations.iter().filter_map(move |(name, value)| {
        if !value.property_type.is_property_type() {
            // Filter away the callbacks
            return None;
        }
        let type_node = value.type_node()?; // skip fake and materialized properties
        let declared_at = file.as_ref().map(|file| {
            let start_position = offset_to_position(type_node.text_range().start().into());
            let uri = lsp_types::Url::from_file_path(file).unwrap_or_else(|_| {
                lsp_types::Url::parse("file:///)").expect("That should have been valid as URL!")
            });

            DeclarationInformation { uri, start_position }
        });
        Some(PropertyInformation {
            name: name.clone(),
            type_name: format!("{}", value.property_type),
            declared_at,
            defined_at: None,
            group: group.to_string(),
        })
    })
}

fn find_expression_range(
    element: &syntax_nodes::Element,
    offset: u32,
    offset_to_position: &mut dyn FnMut(u32) -> lsp_types::Position,
) -> Option<DefinitionInformation> {
    let mut property_definition_range = rowan::TextRange::default();
    let mut expression = rowan::TextRange::default();

    if let Some(token) = element.token_at_offset(offset.into()).right_biased() {
        for ancestor in token.parent_ancestors() {
            if ancestor.kind() == SyntaxKind::BindingExpression {
                // The BindingExpression contains leading and trailing whitespace + `;`
                let expr_range = ancestor
                    .first_child()
                    .expect("A BindingExpression needs to have a child!")
                    .text_range();
                expression = expr_range;
                continue;
            }
            if ancestor.kind() == SyntaxKind::Binding {
                property_definition_range = ancestor.text_range();
                break;
            }
            if ancestor.kind() == SyntaxKind::Element {
                // There should have been a binding before the element!
                break;
            }
        }
    }
    if property_definition_range.start() < expression.start()
        && expression.start() <= expression.end()
        && expression.end() < property_definition_range.end()
    {
        return Some(DefinitionInformation {
            // In the CST, the range end includes the last character, while in the lsp protocol the end of the
            // range is exclusive, i.e. it refers to the first excluded character. Hence the +1 below:
            property_definition_range: crate::util::text_range_to_lsp_range(
                property_definition_range,
                offset_to_position,
            ),
            expression_range: crate::util::text_range_to_lsp_range(expression, offset_to_position),
        });
    } else {
        None
    }
}

fn insert_property_definitions(
    element: &Element,
    properties: &mut Vec<PropertyInformation>,
    offset_to_position: &mut dyn FnMut(u32) -> lsp_types::Position,
) {
    let element_node = element.node.as_ref().expect("Element has to have a node here!");
    let element_range = element_node.text_range();

    for prop_info in properties {
        if let Some(v) = element.bindings.get(prop_info.name.as_str()) {
            if let Some(span) = &v.borrow().span {
                let offset = span.span().offset as u32;
                if element.source_file().map(|sf| sf.path())
                    == span.source_file.as_ref().map(|sf| sf.path())
                    && element_range.contains(offset.into())
                {
                    if let Some(definition) =
                        find_expression_range(element_node, offset, offset_to_position)
                    {
                        prop_info.defined_at = Some(definition);
                    }
                }
            }
        }
    }
}

fn get_properties(
    element: &ElementRc,
    offset_to_position: &mut dyn FnMut(u32) -> lsp_types::Position,
) -> Vec<PropertyInformation> {
    let mut result = vec![];

    result.extend(get_element_properties(&element.borrow(), offset_to_position, ""));
    let mut current_element = element.clone();

    let geometry_prop = HashSet::from(["x", "y", "width", "height"]);

    loop {
        let base_type = current_element.borrow().base_type.clone();
        match base_type {
            ElementType::Component(c) => {
                current_element = c.root_element.clone();
                result.extend(get_element_properties(
                    &current_element.borrow(),
                    offset_to_position,
                    &c.id,
                ));
                continue;
            }
            ElementType::Builtin(b) => {
                result.extend(b.properties.iter().filter_map(|(k, t)| {
                    if geometry_prop.contains(k.as_str()) {
                        // skip geometry property because they are part of the reserved ones
                        return None;
                    }
                    if !t.ty.is_property_type() {
                        // skip callbacks and other functions
                        return None;
                    }

                    Some(PropertyInformation {
                        name: k.clone(),
                        type_name: t.ty.to_string(),
                        declared_at: None,
                        defined_at: None,
                        group: b.name.clone(),
                    })
                }));

                if b.name == "Rectangle" {
                    result.push(PropertyInformation {
                        name: "clip".into(),
                        type_name: Type::Bool.to_string(),
                        declared_at: None,
                        defined_at: None,
                        group: String::new(),
                    });
                }

                result.push(PropertyInformation {
                    name: "opacity".into(),
                    type_name: Type::Float32.to_string(),
                    declared_at: None,
                    defined_at: None,
                    group: String::new(),
                });
                result.push(PropertyInformation {
                    name: "visible".into(),
                    type_name: Type::Bool.to_string(),
                    declared_at: None,
                    defined_at: None,
                    group: String::new(),
                });

                if b.name == "Image" {
                    result.extend(get_reserved_properties(
                        "rotation",
                        i_slint_compiler::typeregister::RESERVED_ROTATION_PROPERTIES,
                    ));
                }

                if b.name == "Rectangle" {
                    result.extend(get_reserved_properties(
                        "drop-shadow",
                        i_slint_compiler::typeregister::RESERVED_DROP_SHADOW_PROPERTIES,
                    ));
                }
            }
            ElementType::Global => {
                break;
            }

            _ => {}
        }

        result.extend(get_reserved_properties(
            "geometry",
            i_slint_compiler::typeregister::RESERVED_GEOMETRY_PROPERTIES,
        ));
        result.extend(
            get_reserved_properties(
                "layout",
                i_slint_compiler::typeregister::RESERVED_LAYOUT_PROPERTIES,
            )
            // padding arbitrary items is not yet implemented
            .filter(|x| !x.name.starts_with("padding")),
        );
        result.push(PropertyInformation {
            name: "accessible-role".into(),
            type_name: Type::Enumeration(
                i_slint_compiler::typeregister::BUILTIN_ENUMS.with(|e| e.AccessibleRole.clone()),
            )
            .to_string(),
            declared_at: None,
            defined_at: None,
            group: "accessibility".into(),
        });
        if element.borrow().is_binding_set("accessible-role", true) {
            result.extend(get_reserved_properties(
                "accessibility",
                i_slint_compiler::typeregister::RESERVED_ACCESSIBILITY_PROPERTIES,
            ));
        }
        break;
    }

    insert_property_definitions(&element.borrow(), &mut result, offset_to_position);

    result
}

fn get_element_information(element: &ElementRc) -> Option<ElementInformation> {
    let e = element.borrow();
    Some(ElementInformation { id: e.id.clone(), type_name: format!("{}", e.base_type) })
}

pub(crate) fn query_properties(
    element: &ElementRc,
    offset_to_position: &mut dyn FnMut(u32) -> lsp_types::Position,
) -> Result<QueryPropertyResponse, crate::Error> {
    Ok(QueryPropertyResponse {
        properties: get_properties(&element, offset_to_position),
        element: get_element_information(&element),
        source_uri: source_file(&element.borrow()),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::test::{complex_document_cache, loaded_document_cache};

    fn find_property<'a>(
        properties: &'a [PropertyInformation],
        name: &'a str,
    ) -> Option<&'a PropertyInformation> {
        properties.iter().find(|p| p.name == name)
    }

    fn properties_at_position_in_cache(
        line: u32,
        character: u32,
        dc: &mut crate::server_loop::DocumentCache,
        url: &lsp_types::Url,
    ) -> Option<Vec<PropertyInformation>> {
        let element = crate::server_loop::element_at_position(
            dc,
            lsp_types::TextDocumentIdentifier { uri: url.clone() },
            lsp_types::Position { line, character },
        )?;
        Some(get_properties(&element, &mut |offset| {
            dc.byte_offset_to_position(offset, url).expect("invalid node offset")
        }))
    }

    fn properties_at_position(line: u32, character: u32) -> Option<Vec<PropertyInformation>> {
        let (mut dc, url, _) = complex_document_cache("fluent");
        properties_at_position_in_cache(line, character, &mut dc, &url)
    }

    #[test]
    fn test_get_properties() {
        let result = properties_at_position(6, 4).unwrap();

        // Property of element:
        assert_eq!(&find_property(&result, "elapsed-time").unwrap().type_name, "duration");
        // Property of base type:
        assert_eq!(&find_property(&result, "no-frame").unwrap().type_name, "bool");
        // reserved properties:
        assert_eq!(
            &find_property(&result, "accessible-role").unwrap().type_name,
            "enum AccessibleRole"
        );

        // Poke deeper:
        let result = properties_at_position(21, 30).unwrap();
        let property = find_property(&result, "background").unwrap();

        let def_at = property.defined_at.as_ref().unwrap();
        assert_eq!(def_at.expression_range.end.line, def_at.expression_range.start.line);
        // -1 because the lsp range end location is exclusive.
        assert_eq!(
            (def_at.expression_range.end.character - 1 - def_at.expression_range.start.character)
                as usize,
            "lightblue".len()
        );
    }

    #[test]
    fn test_get_property_definition() {
        let (mut dc, url, _) = loaded_document_cache("fluent",
            r#"import { LineEdit, Button, Slider, HorizontalBox, VerticalBox } from "std-widgets.slint";

Base1 := Rectangle {
    property<int> foo = 42;
}

Base2 := Base1 {
    foo: 23;
}

MainWindow := Window {
    property <duration> total-time: slider.value * 1s;
    property <duration> elapsed-time;

    callback tick(duration);
    tick(passed-time) => {
        elapsed-time += passed-time;
        elapsed-time = min(elapsed-time, total-time);
    }

    VerticalBox {
        HorizontalBox {
            padding-left: 0;
            Text { text: "Elapsed Time:"; }
            Base2 {
                foo: 15;
                min-width: 200px;
                max-height: 30px;
                background: gray;
                Rectangle {
                    height: 100%;
                    width: parent.width * (elapsed-time/total-time);
                    background: lightblue;
                }
            }
        }
        Text{
            text: (total-time / 1s) + "s";
        }
        HorizontalBox {
            padding-left: 0;
            Text {
                text: "Duration:";
                vertical-alignment: center;
            }
            slider := Slider {
                maximum: 30s / 1s;
                value: 10s / 1s;
                changed(new-duration) => {
                    root.total-time = new-duration * 1s;
                    root.elapsed-time = min(root.elapsed-time, root.total-time);
                }
            }
        }
        Button {
            text: "Reset";
            clicked => {
                elapsed-time = 0
            }
        }
    }
}
            "#.to_string());
        let file_url = url.clone();
        let result = properties_at_position_in_cache(28, 15, &mut dc, &url).unwrap();

        let foo_property = find_property(&result, "foo").unwrap();

        assert_eq!(foo_property.type_name, "int");

        let declaration = foo_property.declared_at.as_ref().unwrap();
        assert_eq!(declaration.uri, file_url);
        assert_eq!(declaration.start_position.line, 3);
        assert_eq!(declaration.start_position.character, 13); // This should probably point to the start of
                                                              // `property<int> foo = 42`, not to the `<`
        assert_eq!(foo_property.group, "Base1");
    }

    #[test]
    fn test_invalid_properties() {
        let (mut dc, url, _) = loaded_document_cache(
            "fluent",
            r#"
global SomeGlobal := {
    property <int> glob: 77;
}

SomeRect := Rectangle {
    foo := InvalidType {
        property <int> abcd: 41;
        width: 45px;
    }
}
            "#
            .to_string(),
        );

        let result = properties_at_position_in_cache(1, 25, &mut dc, &url).unwrap();

        let glob_property = find_property(&result, "glob").unwrap();
        assert_eq!(glob_property.type_name, "int");
        let declaration = glob_property.declared_at.as_ref().unwrap();
        assert_eq!(declaration.uri, url);
        assert_eq!(declaration.start_position.line, 2);
        assert_eq!(glob_property.group, "");
        assert_eq!(find_property(&result, "width"), None);

        let result = properties_at_position_in_cache(8, 4, &mut dc, &url).unwrap();
        let abcd_property = find_property(&result, "abcd").unwrap();
        assert_eq!(abcd_property.type_name, "int");
        let declaration = abcd_property.declared_at.as_ref().unwrap();
        assert_eq!(declaration.uri, url);
        assert_eq!(declaration.start_position.line, 7);
        assert_eq!(abcd_property.group, "");

        let x_property = find_property(&result, "x").unwrap();
        assert_eq!(x_property.type_name, "length");
        assert_eq!(x_property.defined_at, None);
        assert_eq!(x_property.group, "geometry");

        let width_property = find_property(&result, "width").unwrap();
        assert_eq!(width_property.type_name, "length");
        let definition = width_property.defined_at.as_ref().unwrap();
        assert_eq!(definition.expression_range.start.line, 8);
        assert_eq!(width_property.group, "geometry");
    }
}

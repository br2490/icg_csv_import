<?php
/**
 * Utilities for ICG CSV Batch.
 * br2490
 */

/**
 * @param IslandoraFedoraObject $collection
 *
 * @return array
 *   An array describing each model allowed in this collection
 *   - pid: The PID with which the content model object.
 *     - pid: The PID with which the content model object.
 *     - name: The default label to use for new child objects of this type.
 *     - namespace: The default namespace for new child objects of this type.
 *
 * @throws Exception
 */
function _icg_csv_import_get_available_ingest_models(
  IslandoraFedoraObject $collection) {

  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('inc', 'xml_form_builder', 'includes/associations');

  $all_content_models = islandora_get_content_models();

  if (!($policy = $collection['COLLECTION_POLICY'])) {
    throw new Exception('No collection policy was found for this collection.
     Please contact your site admin.');
  }

  $policy = new CollectionPolicy($policy->content);
  $policy_content_models = $policy->getContentModels();

  $ret = array();
  foreach ($policy_content_models as $available_model) {

    // We'll use the pid as the key for our array.
    $pid = $available_model['pid'];

    // Build our return array, keyed by $pid.

    // This provides the human readable name of the cModel.
    $ret['models'][$pid] = $all_content_models[$pid]['label'];

    // This provides the namespace.
    $ret['namespaces'][$pid] = $available_model['namespace'];

    // Grab all associated forms, regardless of DSID.
    $associations = xml_form_builder_get_associations(
      array(), $available_model, array(), FALSE);

    // Build an array of forms, though I think we're only using MODS (?).
    foreach ($associations as $association) {
      $ret['forms'][$pid][] = $association;
    }
  }

  return $ret;
}

/**
 * Small callback for ajax to get associated forms.
 *
 * @param $selected_cmodel
 * @param $xml_forms
 *
 * @return array
 */
function _icg_ajax_callback_xml_form($selected_cmodel, $xml_forms) {
  $ret = array();

  $cmodel_forms = $xml_forms[$selected_cmodel];

  foreach ($cmodel_forms as $form_name) {
    // Do a check here "['enabled']" should be TRUE.
    $name = $form_name['form_name'];
    $ret[$name] = $name;
  }

  return $ret;
}

/**
 * Leaving this here in case it needs to be a SPARQL query.
 *
 * STUB - silly function but here in case this needs to be more robust. Right
 * now we're relying completely on what the COLLECTION_POLICY dictates about
 * namespace; Either we'll need to traverse *->RELS_EXT to find out
 * about all the namespaces... or just SPARQL bright.
 *
 * @param $selected_cmodel
 * @param $namespaces
 *
 * @return array
 */
function _icg_ajax_callback_ns_form($selected_cmodel, $namespaces) {
  $ret = array();

  $ns = $namespaces[$selected_cmodel];
  $ret[$ns] = $ns;

  return $ret;
}

// @Override
/**
 * Gets a Form from the database.
 *
 * Modules should implement xml_form_builder_get_form_modify_definition hook
 * when they need to make modifications form builder schema that is loaded
 * into the form.  This is currently needed to allow for the modifications of
 * tab panel element access so that when it adds a new element the default
 * schema template will have the correct permissions to make the element
 * accessible.
 *
 * @param array $form
 *   An array containing any "wrapping" elements.
 * @param array $form_state
 *   The Drupal Form State
 * @param string $form_name
 *   The Form to render.
 * @param string $xml
 *   The metadata to prepopulate the form with,
 * @param array $parents
 *   An array containing the offset of the parents.
 *
 * @return array
 *   The Drupal Form, if it exists FALSE otherwise.
 */
function _icg_xml_form_builder_get_form(array $form, array &$form_state, $form_name, $options_dom, $xml = NULL, $parents = array()) {
  form_load_include($form_state, 'inc', 'xml_form_builder', 'XMLFormRepository');
  form_load_include($form_state, 'inc', 'xml_form_api', 'XMLForm');
  form_load_include($form_state, 'inc', 'xml_form_api', 'XMLFormDefinition');
  if (!XMLFormRepository::Exists($form_name)) {
    drupal_set_message(t('The form "%name" does not exist.', array('%name' => $form_name)));
    return FALSE;
  }
  elseif (!XMLFormRepository::Valid($form_name)) {
    drupal_set_message(t('The Drupal form "%name" exists but is not valid. Please notify the administrator.', array('%name' => $form_name)));
    return FALSE;
  }
  try {
    $xml_form = new XMLForm($form_state, $parents);
    // Was not initialized from storage.
    if (!$xml_form->isInitialized()) {
       $definition = new XMLFormDefinition(XMLFormRepository::Get($form_name));
      dpm($definition->definition);
      _icg_dom_form_manipulation($definition->definition, $options_dom);

      dpm($definition->definition->saveXML());


      // This can be used later to update saved crosswalks.
      $document = $definition->createXMLDocument($xml);
      $form = $definition->getForm();

      drupal_alter(XML_FORM_BUILDER_GET_FORM_MODIFY_DEFINITION_HOOK, $form,
        $form_state);
      $xml_form->initialize($form, $document);
    }
    $form = array_merge($form, $xml_form->toArray($form_state));
  }
  catch (Exception $e) {
    // Catch any errors that occured and display them to the user so that they
    // may correct their form definition.
    $msg = "File: {$e->getFile()}<br/>Line: {$e->getLine()}<br/>Error: {$e->getMessage()}";
    drupal_set_message(filter_xss($msg), 'error');
    return FALSE;
  }
  return $form;
}

function _icg_dom_form_manipulation(DOMDocument &$definition, Array $mappable_options) {
  static $manipulable_fields = array(
    'textfield',
    'textarea',
    'tags',
  );

//  $options_dom = new DOMDocument();
//  $options = $options_dom->createElement('options');
//  foreach ($headers as $heading) {
//    $option_element = $options_dom->createElement('index', $heading);
//    $option_element->setAttribute("key", $heading);
//    $options->appendChild($option_element);
//  }
//  $options_dom->appendChild($options);
  return;
  $all_types = $definition->getElementsByTagName('type');
  foreach ($all_types as $element) {
    if (in_array($element->nodeValue, $manipulable_fields) && $element instanceof DOMElement) {
      $element->nodeValue = 'select';
      $parent = $element->parentNode;
      $options = $definition->createElement('options');
      foreach ($mappable_options as $option) {
        $option_element = $options->createElement('index', $option);
        $option_element->setAttribute("key", $option);
        $options->appendChild($option_element);
      }
      $options->appendChild($parent);
    }
    dpm($definition->saveXML());
  }
}


/**
 * Iterates through a form array and replaces specific fields and their options.
 *
 * @param $form - The metadata form as an object.
 * @param array $array - The metdata form as an array.
 * @param array $mappable_options - The vector of CSV data to map.
 * @param string $form_name - The metadata form name (or NULL if called from within).
 * 
 * @return array - Corresponding XPaths.  @TODO
 */
function _icg_form_array_iterator_set_options(&$form, &$array, $mappable_options, $form_name=NULL) {
  static $xpaths = array( );
  static $position;

  static $manipulable_fields = array(
    'textfield',
    'textarea',
    'tags',
  );

  // On the first call to this function $form_name should be provided, so use it
  // to fetch all possible xpaths. 
  if (isset($form_name)) {
    $xpaths = _icg_csv_import_get_xpaths($form_name, $array);
  } 
  
  /* @TODO 
   * There is a disconnect here between $xpaths and $form and I haven't been
   * able to determine how to associate elements of $form with their corresponding
   * xpath representations.  Simply tracking $position does not work since there's
   * not a one-to-one correspondence between $xpaths and elements.  For example, $xpaths
   * contains:
   * 
   *    /mods/titleInfo
   *    /mods/titleInfo/title
   * 
   * But /mods/titleInfo is NOT represented by any $form element because only its child,
   * /mods/titleInfo/title is.
   * 
   */
    
  // If this branch of the form is a manipulable field...
  if (isset($array['#type']) && in_array($array['#type'], $manipulable_fields)) {
    $title = $array['#title'];
    $array['#type'] = 'select';
    $array['#options'] = $mappable_options;
    $array['#description'] = 'This field is mapped to ' . $xpaths[$title] . '.';
    $test = str_replace('/mods','',$xpaths[$title]);
    //dpm($test);

    // Add a field for addition of an optional constant.
    $array[$title . '-constant']['#title'] = $title . '-CONST';
    // = array(
      // '#title' = $title . '-CONST',
      // '#description' = 'Enter your constant value for this field',
      // '#type' = 'textfield',
      // '#states' => array(
      //   'invisible' => array(
      //     'input[name='.$title.']' => array(
      //       'value' => 'USE_CONSTANT',
      //     ),
      //   ),
      // ),
    // );
    // @TODO Do we want to append something to the description as help text?
  }
  
  // This branch of the form (tree) is not a manipulable field, break it down further
  else if (is_array($array)) {
    foreach($array as $key => &$value){
      if (isset($value) && is_array($value)) {
        _icg_form_array_iterator_set_options($form, $value, $mappable_options);
      }
    }
  }
  
  return;
}


// This is a modified copy of function xml_form_builder_get_title_options($form_name) 
function _icg_csv_import_get_xpaths($form_name) {
  if (isset($form_name)) {
    module_load_include('inc', 'xml_form_builder', 'XMLFormRepository');
    $doc = XMLFormRepository::Get($form_name);  // This is a DOMDocument representation of the form
    // dpm($doc);

    if ($doc) {
      $xpath = new DOMXPath($doc);
      $result = $xpath->query('//element[@name]');
      // $result = $xpath->query('//element[@name] | //@*');   // failed attempt to get attributes
      $elements = dom_node_list_to_array($result);   // a mix of DOMElement and DOMAttr nodes
      // dpm(get_defined_vars());
      if (count($elements) > 0) {
        $output = array( );
        foreach ($elements as $element) {
          $element_title = NULL;
          $debugXML = $element->ownerDocument->saveXML($element);

          $properties = $element->getElementsByTagName('properties');
          foreach ($properties as $prop) {
            $titles = $prop->getElementsByTagName('title');
            foreach ($titles as $title) {
              $element_title = $title->nodeValue;       // there should be only one
            }
          }

          $result = $xpath->query('ancestor-or-self::node()[@name]', $element);
          $ancestors = dom_node_list_to_array($result);
          $ancestors_array = array();
          if (count($ancestors) > 0) {
            foreach ($ancestors as $ancestor) {
              $name = $ancestor->getAttribute('name');
              $ancestors_array[] = (string) $name;
            }
          }
          $x = "/mods/" . implode('/', $ancestors_array);
          $output[$element_title] = str_replace('/0', '[0]', $x);
        }
        dpm($output);
        return $output;
      }
    }
  }
  return array(NULL => t('Metadata Form Not Found!'));
}

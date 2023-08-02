import { useEffect, useState } from "react";
import axios from 'axios';
import './styles.css';
import LocalizationResults from "../LocalizationResults/LocalizationResults";

function EndResults() {
    const [sourceFiles, setSourceFiles] = useState("");
    const [synchRegions, setSynchRegions] = useState([]);
    
    return ( 
        <div className="container" style={{overflowY: 'scroll'}}>
            <LocalizationResults/>
        </div>
    );
}

export default EndResults;